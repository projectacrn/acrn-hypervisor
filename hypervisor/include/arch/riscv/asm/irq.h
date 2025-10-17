/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_IRQ_H
#define RISCV_IRQ_H

#include <cpu.h>

#define IPI_NOTIFY_CPU		0U

/* CPU-local interrupt count (room for standard causes) */
#define IRQ_NUM_CPU_DOMAIN	16U

/*
 * TODO: Abstract IRQ_NUM_PLIC_DOMAIN from the device tree.
 *
 * Example PLIC device tree entry (as emulated by QEMU):
 *       plic@c000000 {
 *           ...
 *           riscv,ndev = <0x0000005f>;
 *           ...
 * };
 *
 * Add 1 to 0x0000005f since interrupt ID 0 is reserved to mean "no interrupt",
 * per the PLIC specification.
 */
#define IRQ_NUM_PLIC_DOMAIN	0x60U

/*
 * Future: IRQ_NUM_APLIC_DOMAIN
 *
 * Example APLIC device tree entry:
 *       aplic1: interrupt-controller@d000000 {
 *              ...
 *              riscv,num-sources = <63>;
 *              ...
 *       };
 */

#define NR_IRQS			(IRQ_NUM_CPU_DOMAIN + IRQ_NUM_PLIC_DOMAIN)

struct riscv_irq_data {
	uint32_t acrn_irq;
	/* Can be extended when PROFILING_ON is supported, similar to "struct x86_irq_data". */
};

struct intr_excp_ctx {
	struct cpu_regs regs;
};

/* IRQ domain names */
#define RISCV_IRQD_CPU		"cpu-intc"

/*
 * For non-CPU IRQ domains, the domain name can be derived from the device tree
 * during the IRQ chip probe.
 *
 * Example device tree:
 *
 *   IRQ chip:
 *     plic0: interrupt-controller@c000000 {
 *         compatible = "sifive,fu540-c000-plic", "sifive,plic-1.0.0";
 *         ...
 *     };
 *
 *   Device:
 *     i2c0: i2c@10030000 {
 *         ...
 *         interrupt-parent = <&plic0>;
 *         ...
 *     };
 *
 * The IRQ domain name can be taken from the 'interrupt-parent' label (e.g. "plic0"),
 * or derived by combining it with one of the compatible strings when only a phandle
 * is provided as the 'interrupt-parent'. This helps uniquely identify each IRQ
 * domain instance.
 */

/* Domain registration & flat mapping helpers */
bool riscv_register_irq_domain(const char *name, uint32_t irq_num);
uint32_t riscv_domain_get_acrn_irq(const char *name, uint32_t src_id);

/* Helper to verify that the IRQ is registered and valid during interrupt dispatch. */
bool riscv_is_valid_acrn_irq(uint32_t irq);

#endif /* RISCV_IRQ_H */
