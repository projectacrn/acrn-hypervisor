/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/trap.h>
#include <cpu.h>
#include <asm/csr.h>
#include <irq.h>
#include <rtl.h>
#include <spinlock.h>
#include <types.h>

/**
 * RISC-V IRQ integration with the common IRQ framework (simplified flat mapping).
 *
 * IRQ Domain Mapping Overview
 * ===========================
 *
 *              +---------------------------------------+
 *              |         Common IRQ Framework          |
 *              |---------------------------------------|
 *              | IRQ table: irq_desc_array[]           |
 *              | Indexed by acrn_irq                   |
 *              +---------------------------------------+
 *                            ^
 *                            |
 *                            |  acrn_irq = domain_base + src_id
 *                            |
 *        +-------------------+-------------------------------+
 *        |                   |                               |
 *   +-----------+        +-----------+                   +-----------+
 *   | CPU IRQ   |        |   PLIC    |                   |   Other   |
 *   | Domain    |        |  Domain   |                   |  Domain   |
 *   +-----------+        +-----------+                   +-----------+
 *        |                    |                               |
 *        | src_id = trap      | src_id = PLIC source ID       |
 *        | cause              |                               |
 *        v                    v                               v
 *   +-----------+        +-----------+                   +-----------+
 *   | SW IRQ,   |        | Ext. Dev  |                   | Ext. Dev  |
 *   | Timer,    |        | Interrupt |                   | Interrupt |
 *   | etc.      |        +-----------+                   +-----------+
 *   +-----------+
 *
 * Flat offset-based model:
 *   acrn_irq = domain_base + src_id;
 *
 * - domain_base: the base ACRN IRQ for this domain, allocated by
 *                riscv_register_irq_domain() during domain initialization.
 * - src_id: the IRQ source ID within a specific IRQ domain.
 *   For example:
 *     - In the CPU IRQ domain, src_id corresponds to the trap cause.
 *     - In the PLIC IRQ domain, src_id corresponds to the external source ID
 *       defined in the device tree.
 *
 * Example device tree entry for a device whose interrupt is routed through the PLIC:
 *     i2c0: i2c@10030000 {
 *         ...
 *         interrupt-parent = <&plic0>;
 *         interrupts = <52>; // src_id is 52 for this device.
 *         ...
 *     };
 *
 * Workflow overview:
 *
 * By the IRQ chip:
 * 1. Domain registration and IRQ reservation:
 *    riscv_register_irq_domain(domain_name, irq_num);
 *      - The CPU IRQ domain is registered first with base 0.
 *      - Other IRQ domains can be registered later during IRQ chip probe.
 *
 * By the specific device:
 * 2. IRQ handler registration:
 *    // Lookup the ACRN IRQ based on domain name and src_id.
 *    acrn_irq = riscv_domain_get_acrn_irq(domain_name, src_id);
 *    // Register the IRQ handler.
 *    request_irq(acrn_irq, irq_handler, NULL, IRQF_NONE);
 *
 */

static spinlock_t riscv_irq_lock = { .head = 0U, .tail = 0U };

static struct riscv_irq_data irq_data[NR_IRQS];

/* Maximum number of IRQ domains */
#define MAX_IRQ_DOMAIN_NUM		2U

/* Maximum name size of IRQ domain */
#define MAX_IRQ_DOMAIN_NAME_SIZE	32U

/* IRQ domain registry */
struct riscv_irq_domain {
	char name[MAX_IRQ_DOMAIN_NAME_SIZE]; /* name of this domain */
	uint32_t base;    /* base acrn_irq for this domain */
	uint32_t irq_num; /* number of IRQs in this domain */
};

static struct riscv_irq_domain riscv_domains[MAX_IRQ_DOMAIN_NUM];
static uint32_t nr_domains; /* number of registered domains */

/*
 * Find a domain by name.
 * Return: pointer to the domain if found, NULL otherwise.
 */
static struct riscv_irq_domain *find_domain_by_name(const char *name)
{
	uint32_t i;
	struct riscv_irq_domain *domain = NULL;

	/* Check if name is valid */
	if (name == NULL) {
		pr_err("%s: invalid name", __func__);
	} else {
		for (i = 0U; i < nr_domains; i++) {
			if (strncmp(riscv_domains[i].name, name, MAX_IRQ_DOMAIN_NAME_SIZE) == 0) {
				domain = &riscv_domains[i];
				break;
			}
		}
	}

	return domain;
}

/*
 * Return the ACRN IRQ for the given domain and source ID (acrn_irq = domain_base + src_id).
 * Return IRQ_INVALID if the given domain or source ID is invalid.
 */
uint32_t riscv_domain_get_acrn_irq(const char *name, uint32_t src_id)
{
	uint32_t acrn_irq = IRQ_INVALID;
	struct riscv_irq_domain *domain = NULL;

	domain = find_domain_by_name(name);
	if ((domain != NULL) && (src_id < domain->irq_num)) {
		acrn_irq = domain->base + src_id;
	} else {
		pr_err("%s: invalid params name=%s src_id=%u", __func__, name, src_id);
	}

	return acrn_irq;
}

/*
 * Register an IRQ domain and reserve ACRN IRQs for that domain.
 *
 * Return: false if domain registration or IRQ reservation fails.
 */
bool riscv_register_irq_domain(const char *name, uint32_t irq_num)
{
	static uint32_t next_acrn_irq_base; /* next free base for auto-allocation */
	bool ret = false;
	uint32_t base, end, i, acrn_irq;
	uint64_t flags;
	struct riscv_irq_domain *domain = NULL;

	/* Check if domain with this name already exists */
	if (find_domain_by_name(name) != NULL) {
		pr_err("%s: domain %s already exists", __func__, name);
	/* Check if we have space for a new domain */
	} else if (nr_domains >= MAX_IRQ_DOMAIN_NUM) {
		pr_err("%s: registered domains (%d) exceeds MAX_IRQ_DOMAIN_NUM (%d)",
			__func__, nr_domains, MAX_IRQ_DOMAIN_NUM);
	} else {
		spinlock_irqsave_obtain(&riscv_irq_lock, &flags);

		base = next_acrn_irq_base;
		end = base + irq_num;

		/* Domain registration */
		if ((irq_num > 0U) && (end <= NR_IRQS)) {
			domain = &riscv_domains[nr_domains];

			(void)strncpy_s(domain->name, MAX_IRQ_DOMAIN_NAME_SIZE, name, (MAX_IRQ_DOMAIN_NAME_SIZE - 1U));
			domain->base = base;
			domain->irq_num = irq_num;

			nr_domains++;
			next_acrn_irq_base = end;
			ret = true;
		} else {
			pr_err("%s: invalid params name=%s base=%u irq_num=%u (NR_IRQS=%u next_base=%u)",
				__func__, name, base, irq_num, NR_IRQS, next_acrn_irq_base);
		}

		spinlock_irqrestore_release(&riscv_irq_lock, flags);
	}

	/* IRQ reservation */
	if (ret) {
		for (i = 0U; i < irq_num; i++) {
			acrn_irq = riscv_domain_get_acrn_irq(name, i);

			if ((acrn_irq == IRQ_INVALID) || (reserve_irq_num(acrn_irq) == IRQ_INVALID)) {
				pr_err("%s: failed to reserve IRQ[%d]", __func__, acrn_irq);
				ret = false;
				break;
			}
		}
	}

	return ret;
}

bool arch_request_irq(__unused uint32_t irq)
{
	bool ret = false;
	uint64_t flags;

	spinlock_irqsave_obtain(&riscv_irq_lock, &flags);

	if (irq < NR_IRQS) {
		/*
		 * Update irq_data[] during request_irq().
		 * The data is later used by riscv_is_valid_acrn_irq() to verify
		 * whether a given ACRN IRQ is valid when dispatching interrupts.
		 */
		irq_data[irq].acrn_irq = irq;
		ret = true;
	} else {
		pr_err("invalid irq=%u", irq);
	}

	spinlock_irqrestore_release(&riscv_irq_lock, flags);

	return ret;
}

void arch_free_irq(__unused uint32_t irq)
{
	uint64_t flags;

	spinlock_irqsave_obtain(&riscv_irq_lock, &flags);

	if (irq < NR_IRQS) {
		irq_data[irq].acrn_irq = IRQ_INVALID;
	} else {
		pr_err("invalid irq=%u", irq);
	}

	spinlock_irqrestore_release(&riscv_irq_lock, flags);
}

void arch_pre_irq(__unused const struct irq_desc *desc)
{
	/* No pre-dispatch actions */
}

void arch_post_irq(__unused const struct irq_desc *desc)
{
	/* No post-dispatch actions */
}

void arch_init_irq_descs(struct irq_desc descs[])
{
	uint32_t i;

	/* Attach arch_data pointer */
	for (i = 0U; i < NR_IRQS; i++) {
		irq_data[i].acrn_irq = IRQ_INVALID;
		descs[i].arch_data = &irq_data[i];
	}
}

void arch_setup_irqs(void)
{
	/* The CPU IRQ domain is mandatory and registered during interrupt initialization. */
	riscv_register_irq_domain(RISCV_IRQD_CPU, IRQ_NUM_CPU_DOMAIN);

	/**
	 * NOTE:
	 * Additional domains (PLIC/APLIC) will be registered later by IRQ chip drivers.
	 *
	 * Example:
	 *   riscv_register_irq_domain(RISCV_IRQD_PLIC, plic_ndev);
	 */
}

void arch_init_interrupt(__unused uint16_t pcpu_id)
{
	uint64_t addr = (uint64_t)&strap_handler;

	/*
	 * According to RISC-V Privileged Architecture
	 * 12.1.2. Supervisor Trap Vector Base Address (stvec) Register:
	 * The BASE field in stvec is a field that can hold any valid virtual
	 * or physical address, subject to the following alignment constraints:
	 * the address must be ``4-byte aligned``, and MODE settings other than
	 * Direct might impose additional alignment constraints on the
	 * value in the BASE field.
	 */
	cpu_csr_write(CSR_STVEC, (addr | TRAP_VECTOR_MODE_DIRECT));
	cpu_csr_write(CSR_SIE, (IP_IE_SSI | IP_IE_STI | IP_IE_SEI));
}

bool riscv_is_valid_acrn_irq(uint32_t irq)
{
	bool ret = false;

	if (irq < NR_IRQS) {
		ret = (irq_data[irq].acrn_irq == irq);
	}

	return ret;
}
