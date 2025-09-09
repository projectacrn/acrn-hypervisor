/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <bits.h>
#include <spinlock.h>
#include <per_cpu.h>
#include <io.h>
#include <asm/irq.h>
#include <asm/idt.h>
#include <asm/ioapic.h>
#include <asm/lapic.h>
#include <dump.h>
#include <logmsg.h>
#include <asm/vmx.h>

static spinlock_t x86_irq_spinlock = { .head = 0U, .tail = 0U, };

static struct x86_irq_data irq_data[NR_IRQS];

static uint32_t vector_to_irq[NR_MAX_VECTOR + 1];

typedef void (*spurious_handler_t)(uint32_t vector);

spurious_handler_t spurious_handler;

static struct {
	uint32_t irq;
	uint32_t vector;
} irq_static_mappings[NR_STATIC_MAPPINGS] = {
	{TIMER_IRQ, TIMER_VECTOR},
	{THERMAL_IRQ, THERMAL_VECTOR},
	{NOTIFY_VCPU_IRQ, NOTIFY_VCPU_VECTOR},
	{PMI_IRQ, PMI_VECTOR},

	/* To be initialized at runtime in init_irq_descs() */
	[NR_STATIC_MAPPINGS_1 ... (NR_STATIC_MAPPINGS - 1U)] = {},
};

/*
 * allocate a vector and bind it to irq
 * for legacy_irq (irq num < 16) and static mapped ones, do nothing
 * if mapping is correct.
 * retval: valid vector number on success, VECTOR_INVALID on failure.
 */
uint32_t alloc_irq_vector(uint32_t irq)
{
	struct x86_irq_data *irqd;
	uint64_t rflags;
	uint32_t vr = VECTOR_INVALID;
	uint32_t ret = VECTOR_INVALID;

	if (irq < NR_IRQS) {
		irqd = &irq_data[irq];
		spinlock_irqsave_obtain(&x86_irq_spinlock, &rflags);

		if (irqd->vector != VECTOR_INVALID) {
			if (vector_to_irq[irqd->vector] == irq) {
				/* statically binded */
				vr = irqd->vector;
			} else {
				pr_err("[%s] irq[%u]:vector[%u] mismatch",
					__func__, irq, irqd->vector);
			}
		} else {
			/* alloc a vector between:
			 *   VECTOR_DYNAMIC_START ~ VECTOR_DYNAMIC_END
			 */
			for (vr = VECTOR_DYNAMIC_START;
				vr <= VECTOR_DYNAMIC_END; vr++) {
				if (vector_to_irq[vr] == IRQ_INVALID) {
					irqd->vector = vr;
					vector_to_irq[vr] = irq;
					break;
				}
			}
			vr = (vr > VECTOR_DYNAMIC_END) ? VECTOR_INVALID : vr;
		}
		spinlock_irqrestore_release(&x86_irq_spinlock, rflags);
		ret = vr;
	} else {
		pr_err("invalid irq[%u] to alloc vector", irq);
	}

	return ret;
}

bool request_irq_arch(uint32_t irq)
{
	return (alloc_irq_vector(irq) != VECTOR_INVALID);
}

/* free the vector allocated via alloc_irq_vector() */
static void free_irq_vector(uint32_t irq)
{
	struct x86_irq_data *irqd;
	uint32_t vr;
	uint64_t rflags;

	if (irq < NR_IRQS) {
		irqd = &irq_data[irq];
		spinlock_irqsave_obtain(&x86_irq_spinlock, &rflags);

		if (irqd->vector < VECTOR_FIXED_START) {
			/* do nothing for LEGACY_IRQ and static allocated ones */
			vr = irqd->vector;
			irqd->vector = VECTOR_INVALID;

			if ((vr <= NR_MAX_VECTOR) && (vector_to_irq[vr] == irq)) {
				vector_to_irq[vr] = IRQ_INVALID;
			}
		}
		spinlock_irqrestore_release(&x86_irq_spinlock, rflags);
	}
}

void free_irq_arch(uint32_t irq)
{
	free_irq_vector(irq);
}

uint32_t irq_to_vector(uint32_t irq)
{
	uint64_t rflags;
	uint32_t ret = VECTOR_INVALID;

	if (irq < NR_IRQS) {
		spinlock_irqsave_obtain(&x86_irq_spinlock, &rflags);
		ret = irq_data[irq].vector;
		spinlock_irqrestore_release(&x86_irq_spinlock, rflags);
	}

	return ret;
}

static void handle_spurious_interrupt(uint32_t vector)
{
	send_lapic_eoi();

	get_cpu_var(spurious)++;

	pr_warn("Spurious vector: 0x%x.", vector);

	if (spurious_handler != NULL) {
		spurious_handler(vector);
	}
}

static inline bool irq_need_mask(const struct irq_desc *desc)
{
	/* level triggered gsi should be masked */
	return (((desc->flags & IRQF_LEVEL) != 0U)
		&& is_ioapic_irq(desc->irq));
}

static inline bool irq_need_unmask(const struct irq_desc *desc)
{
	/* level triggered gsi for non-ptdev should be unmasked */
	return (((desc->flags & IRQF_LEVEL) != 0U)
		&& ((desc->flags & IRQF_PT) == 0U)
		&& is_ioapic_irq(desc->irq));
}

void pre_irq_arch(const struct irq_desc *desc)
{
	if (irq_need_mask(desc))  {
		ioapic_gsi_mask_irq(desc->irq);
	}

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();
}

void post_irq_arch(const struct irq_desc *desc)
{
	if (irq_need_unmask(desc)) {
		ioapic_gsi_unmask_irq(desc->irq);
	}
}

void dispatch_interrupt(const struct intr_excp_ctx *ctx)
{
	uint32_t vr = ctx->vector;
	uint32_t irq = vector_to_irq[vr];
	struct x86_irq_data *irqd;

	/* The value from vector_to_irq[] must be:
	 * IRQ_INVALID, which means the vector is not allocated;
	 * or
	 * < NR_IRQS, which is the irq number it bound with;
	 * Any other value means there is something wrong.
	 */
	if (irq < NR_IRQS) {
		irqd = &irq_data[irq];

		if (vr == irqd->vector) {
#ifdef PROFILING_ON
			/* Saves ctx info into irq_desc */
			irqd->ctx_rip = ctx->rip;
			irqd->ctx_rflags = ctx->rflags;
			irqd->ctx_cs = ctx->cs;
#endif
			/* Call the generic IRQ handling routine */
			do_irq(irq);
		}
	} else {
		handle_spurious_interrupt(vr);
	}
}

/*
 * descs[] must have NR_IRQS entries
 */
void init_irq_descs_arch(struct irq_desc descs[])
{
	uint32_t i;

	/*
	 * Fill in #CONFIG_MAX_VM_NUM posted interrupt specific irq and vector pairs
	 * at runtime
	 */
	for (i = 0U; i < CONFIG_MAX_VM_NUM; i++) {
		uint32_t idx = i + NR_STATIC_MAPPINGS_1;

		ASSERT(irq_static_mappings[idx].irq == 0U, "");
		ASSERT(irq_static_mappings[idx].vector == 0U, "");

		irq_static_mappings[idx].irq = POSTED_INTR_IRQ + i;
		irq_static_mappings[idx].vector = POSTED_INTR_VECTOR + i;
	}

	for (i = 0U; i < NR_IRQS; i++) {
		irq_data[i].vector = VECTOR_INVALID;
		descs[i].arch_data = &irq_data[i];
	}

	for (i = 0U; i <= NR_MAX_VECTOR; i++) {
		vector_to_irq[i] = IRQ_INVALID;
	}

	/* init fixed mapping for specific irq and vector */
	for (i = 0U; i < NR_STATIC_MAPPINGS; i++) {
		uint32_t irq = irq_static_mappings[i].irq;
		uint32_t vr = irq_static_mappings[i].vector;

		irq_data[irq].vector = vr;
		vector_to_irq[vr] = irq;

		reserve_irq_num(irq);
	}
}

/* must be called after IRQ setup */
void setup_irqs_arch(void)
{
	ioapic_setup_irqs();
}

static void disable_pic_irqs(void)
{
	pio_write8(0xffU, 0xA1U);
	pio_write8(0xffU, 0x21U);
}

static inline void fixup_idt(const struct host_idt_descriptor *idtd)
{
	uint32_t i;
	struct idt_64_descriptor *idt_desc = idtd->idt->host_idt_descriptors;
	uint32_t entry_hi_32, entry_lo_32;

	for (i = 0U; i < HOST_IDT_ENTRIES; i++) {
		entry_lo_32 = idt_desc[i].offset_63_32;
		entry_hi_32 = idt_desc[i].rsvd;
		idt_desc[i].rsvd = 0U;
		idt_desc[i].offset_63_32 = entry_hi_32;
		idt_desc[i].high32.bits.offset_31_16 = (uint16_t)(entry_lo_32 >> 16U);
		idt_desc[i].low32.bits.offset_15_0 = (uint16_t)entry_lo_32;
	}
}

static inline void set_idt(struct host_idt_descriptor *idtd)
{
	asm volatile ("   lidtq %[idtd]\n" :	/* no output parameters */
		      :		/* input parameters */
		      [idtd] "m"(*idtd));
}

void init_interrupt_arch(uint16_t pcpu_id)
{
	struct host_idt_descriptor *idtd = &HOST_IDTR;

	if (pcpu_id == BSP_CPU_ID) {
		fixup_idt(idtd);
	}
	set_idt(idtd);
	init_lapic(pcpu_id);

	if (pcpu_id == BSP_CPU_ID) {
		/* we use ioapic only, disable legacy PIC */
		disable_pic_irqs();
	}
}
