/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <spinlock.h>
#include <per_cpu.h>
#include <io.h>
#include <irq.h>
#include <idt.h>
#include <ioapic.h>
#include <lapic.h>
#include <softirq.h>
#include <vboot.h>
#include <dump.h>
#include <logmsg.h>

static spinlock_t exception_spinlock = { .head = 0U, .tail = 0U, };
static spinlock_t irq_alloc_spinlock = { .head = 0U, .tail = 0U, };

uint64_t irq_alloc_bitmap[IRQ_ALLOC_BITMAP_SIZE];
struct irq_desc irq_desc_array[NR_IRQS];
static uint32_t vector_to_irq[NR_MAX_VECTOR + 1];

spurious_handler_t spurious_handler;

struct static_mapping_table {
	uint32_t irq;
	uint32_t vector;
};

static struct static_mapping_table irq_static_mappings[NR_STATIC_MAPPINGS] = {
	{TIMER_IRQ, VECTOR_TIMER},
	{NOTIFY_IRQ, VECTOR_NOTIFY_VCPU},
	{POSTED_INTR_NOTIFY_IRQ, VECTOR_POSTED_INTR},
	{PMI_IRQ, VECTOR_PMI},
};

/*
 * alloc an free irq if req_irq is IRQ_INVALID, or else set assigned
 * return: irq num on success, IRQ_INVALID on failure
 */
uint32_t alloc_irq_num(uint32_t req_irq)
{
	uint32_t irq = req_irq;
	uint64_t rflags;
	uint32_t ret;

	if ((irq >= NR_IRQS) && (irq != IRQ_INVALID)) {
		pr_err("[%s] invalid req_irq %u", __func__, req_irq);
	        ret = IRQ_INVALID;
	} else {
		spinlock_irqsave_obtain(&irq_alloc_spinlock, &rflags);
		if (irq == IRQ_INVALID) {
			/* if no valid irq num given, find a free one */
			irq = (uint32_t)ffz64_ex(irq_alloc_bitmap, NR_IRQS);
		}

		if (irq >= NR_IRQS) {
			irq = IRQ_INVALID;
		} else {
			bitmap_set_nolock((uint16_t)(irq & 0x3FU),
					irq_alloc_bitmap + (irq >> 6U));
		}
		spinlock_irqrestore_release(&irq_alloc_spinlock, rflags);
		ret = irq;
	}
	return ret;
}

/*
 * @pre: irq is not in irq_static_mappings
 * free irq num allocated via alloc_irq_num()
 */
static void free_irq_num(uint32_t irq)
{
	uint64_t rflags;

	if (irq < NR_IRQS) {
		if (!ioapic_irq_is_gsi(irq)) {
			spinlock_irqsave_obtain(&irq_alloc_spinlock, &rflags);
			(void)bitmap_test_and_clear_nolock((uint16_t)(irq & 0x3FU),
						     irq_alloc_bitmap + (irq >> 6U));
			spinlock_irqrestore_release(&irq_alloc_spinlock, rflags);
		}
	}
}

/*
 * alloc an vectror and bind it to irq
 * for legacy_irq (irq num < 16) and static mapped ones, do nothing
 * if mapping is correct.
 * retval: valid vector num on susccess, VECTOR_INVALID on failure.
 */
uint32_t alloc_irq_vector(uint32_t irq)
{
	uint32_t vr;
	struct irq_desc *desc;
	uint64_t rflags;
	uint32_t ret;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];

		if (desc->vector != VECTOR_INVALID) {
			if (vector_to_irq[desc->vector] == irq) {
				/* statically binded */
				vr = desc->vector;
			} else {
				pr_err("[%s] irq[%u]:vector[%u] mismatch",
					__func__, irq, desc->vector);
				vr = VECTOR_INVALID;
			}
		} else {
			/* alloc a vector between:
			 *   VECTOR_DYNAMIC_START ~ VECTOR_DYNAMC_END
			 */
			spinlock_irqsave_obtain(&irq_alloc_spinlock, &rflags);

			for (vr = VECTOR_DYNAMIC_START;
				vr <= VECTOR_DYNAMIC_END; vr++) {
				if (vector_to_irq[vr] == IRQ_INVALID) {
					desc->vector = vr;
					vector_to_irq[vr] = irq;
					break;
				}
			}
			vr = (vr > VECTOR_DYNAMIC_END) ? VECTOR_INVALID : vr;

			spinlock_irqrestore_release(&irq_alloc_spinlock, rflags);
		}
		ret = vr;
	} else {
		pr_err("invalid irq[%u] to alloc vector", irq);
	        ret = VECTOR_INVALID;
	}
	return ret;
}

/* free the vector allocated via alloc_irq_vector() */
static void free_irq_vector(uint32_t irq)
{
	struct irq_desc *desc;
	uint32_t vr;
	uint64_t rflags;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];

		if ((irq >= NR_LEGACY_IRQ) && (desc->vector < VECTOR_FIXED_START)) {
			/* do nothing for LEGACY_IRQ and static allocated ones */

			spinlock_irqsave_obtain(&irq_alloc_spinlock, &rflags);
			vr = desc->vector;
			desc->vector = VECTOR_INVALID;

			vr &= NR_MAX_VECTOR;
			if (vector_to_irq[vr] == irq) {
				vector_to_irq[vr] = IRQ_INVALID;
			}
			spinlock_irqrestore_release(&irq_alloc_spinlock, rflags);
		}
	}
}

/*
 * There are four cases as to irq/vector allocation:
 * case 1: req_irq = IRQ_INVALID
 *      caller did not know which irq to use, and want system to
 *      allocate available irq for it. These irq are in range:
 *      nr_gsi ~ NR_IRQS
 *      an irq will be allocated and a vector will be assigned to this
 *      irq automatically.
 * case 2: req_irq >= NR_LAGACY_IRQ and irq < nr_gsi
 *      caller want to add device ISR handler into ioapic pins.
 *      a vector will automatically assigned.
 * case 3: req_irq >=0 and req_irq < NR_LEGACY_IRQ
 *      caller want to add device ISR handler into ioapic pins, which
 *      is a legacy irq, vector already reserved.
 *      Nothing to do in this case.
 * case 4: irq with speical type (not from IOAPIC/MSI)
 *      These irq value are pre-defined for Timer, IPI, Spurious etc,
 *      which is listed in irq_static_mappings[].
 *	Nothing to do in this case.
 *
 * return value: valid irq (>=0) on success, otherwise errno (< 0).
 */
int32_t request_irq(uint32_t req_irq, irq_action_t action_fn, void *priv_data,
			uint32_t flags)
{
	struct irq_desc *desc;
	uint32_t irq, vector;
	uint64_t rflags;
	int32_t ret;

	irq = alloc_irq_num(req_irq);
	if (irq == IRQ_INVALID) {
		pr_err("[%s] invalid irq num", __func__);
		ret = -EINVAL;
	} else {
		vector = alloc_irq_vector(irq);

		if (vector == VECTOR_INVALID) {
			pr_err("[%s] failed to alloc vector for irq %u",
				__func__, irq);
			free_irq_num(irq);
			ret = -EINVAL;
		} else {
			desc = &irq_desc_array[irq];
			spinlock_irqsave_obtain(&desc->lock, &rflags);
			if (desc->action == NULL) {
				desc->flags = flags;
				desc->priv_data = priv_data;
				desc->action = action_fn;
				spinlock_irqrestore_release(&desc->lock, rflags);

				ret = (int32_t)irq;
				dev_dbg(ACRN_DBG_IRQ, "[%s] irq%d vr:0x%x", __func__, irq, desc->vector);
			} else {
				spinlock_irqrestore_release(&desc->lock, rflags);

				ret = -EBUSY;
				pr_err("%s: request irq(%u) vr(%u) failed, already requested", __func__,
						irq, irq_to_vector(irq));
			}
		}
	}

	return ret;
}

void free_irq(uint32_t irq)
{
	uint64_t rflags;
	struct irq_desc *desc;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];
		dev_dbg(ACRN_DBG_IRQ, "[%s] irq%d vr:0x%x",
			__func__, irq, irq_to_vector(irq));

		free_irq_vector(irq);
		free_irq_num(irq);

		spinlock_irqsave_obtain(&desc->lock, &rflags);
		desc->action = NULL;
		desc->priv_data = NULL;
		desc->flags = IRQF_NONE;
		spinlock_irqrestore_release(&desc->lock, rflags);
	}
}

void set_irq_trigger_mode(uint32_t irq, bool is_level_triggered)
{
	uint64_t rflags;
	struct irq_desc *desc;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];
		spinlock_irqsave_obtain(&desc->lock, &rflags);
		if (is_level_triggered == true) {
			desc->flags |= IRQF_LEVEL;
		} else {
			desc->flags &= ~IRQF_LEVEL;
		}
		spinlock_irqrestore_release(&desc->lock, rflags);
	}
}

uint32_t irq_to_vector(uint32_t irq)
{
	uint32_t ret;
	if (irq < NR_IRQS) {
		ret = irq_desc_array[irq].vector;
	} else {
	        ret = VECTOR_INVALID;
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
		&& ioapic_irq_is_gsi(desc->irq));
}

static inline bool irq_need_unmask(const struct irq_desc *desc)
{
	/* level triggered gsi for non-ptdev should be unmasked */
	return (((desc->flags & IRQF_LEVEL) != 0U)
		&& ((desc->flags & IRQF_PT) == 0U)
		&& ioapic_irq_is_gsi(desc->irq));
}

static inline void handle_irq(const struct irq_desc *desc)
{
	irq_action_t action = desc->action;

	if (irq_need_mask(desc))  {
		ioapic_gsi_mask_irq(desc->irq);
	}

	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();

	if (action != NULL) {
		action(desc->irq, desc->priv_data);
	}

	if (irq_need_unmask(desc)) {
		ioapic_gsi_unmask_irq(desc->irq);
	}
}

/* do_IRQ() */
void dispatch_interrupt(const struct intr_excp_ctx *ctx)
{
	uint32_t vr = ctx->vector;
	uint32_t irq = vector_to_irq[vr];
	struct irq_desc *desc;

	/* The value from vector_to_irq[] must be:
	 * IRQ_INVALID, which means the vector is not allocated;
	 * or
	 * < NR_IRQS, which is the irq number it bound with;
	 * Any other value means there is something wrong.
	 */
	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];
		per_cpu(irq_count, get_pcpu_id())[irq]++;

		if (vr == desc->vector &&
			bitmap_test((uint16_t)(irq & 0x3FU), irq_alloc_bitmap + (irq >> 6U)) != 0U) {
#ifdef PROFILING_ON
			/* Saves ctx info into irq_desc */
			desc->ctx_rip = ctx->rip;
			desc->ctx_rflags = ctx->rflags;
			desc->ctx_cs = ctx->cs;
#endif
			handle_irq(desc);
		}
	} else {
		handle_spurious_interrupt(vr);
	}
}

void dispatch_exception(struct intr_excp_ctx *ctx)
{
	uint16_t pcpu_id = get_pcpu_id();

	/* Obtain lock to ensure exception dump doesn't get corrupted */
	spinlock_obtain(&exception_spinlock);

	/* Dump exception context */
	dump_exception(ctx, pcpu_id);

	/* Release lock to let other CPUs handle exception */
	spinlock_release(&exception_spinlock);

	/* Halt the CPU */
	cpu_dead();
}

static void init_irq_descs(void)
{
	uint32_t i;

	for (i = 0U; i < NR_IRQS; i++) {
		irq_desc_array[i].irq = i;
		irq_desc_array[i].vector = VECTOR_INVALID;
		spinlock_init(&irq_desc_array[i].lock);
	}

	for (i = 0U; i <= NR_MAX_VECTOR; i++) {
		vector_to_irq[i] = IRQ_INVALID;
	}

	/* init fixed mapping for specific irq and vector */
	for (i = 0U; i < NR_STATIC_MAPPINGS; i++) {
		uint32_t irq = irq_static_mappings[i].irq;
		uint32_t vr = irq_static_mappings[i].vector;

		irq_desc_array[irq].vector = vr;
		vector_to_irq[vr] = irq;
		bitmap_set_nolock((uint16_t)(irq & 0x3FU),
			      irq_alloc_bitmap + (irq >> 6U));
	}
}

static void disable_pic_irqs(void)
{
	pio_write8(0xffU, 0xA1U);
	pio_write8(0xffU, 0x21U);
}

void init_default_irqs(uint16_t cpu_id)
{
	if (cpu_id == BOOT_CPU_ID) {
		init_irq_descs();

		/* we use ioapic only, disable legacy PIC */
		disable_pic_irqs();
		ioapic_setup_irqs();
		init_softirq();
	}
}

static inline void fixup_idt(const struct host_idt_descriptor *idtd)
{
	uint32_t i;
	union idt_64_descriptor *idt_desc = (union idt_64_descriptor *)idtd->idt;
	uint32_t entry_hi_32, entry_lo_32;

	for (i = 0U; i < HOST_IDT_ENTRIES; i++) {
		entry_lo_32 = idt_desc[i].fields.offset_63_32;
		entry_hi_32 = idt_desc[i].fields.rsvd;
		idt_desc[i].fields.rsvd = 0U;
		idt_desc[i].fields.offset_63_32 = entry_hi_32;
		idt_desc[i].fields.high32.bits.offset_31_16 = entry_lo_32 >> 16U;
		idt_desc[i].fields.low32.bits.offset_15_0 = entry_lo_32 & 0xffffUL;
	}
}

static inline void set_idt(struct host_idt_descriptor *idtd)
{
	asm volatile ("   lidtq %[idtd]\n" :	/* no output parameters */
		      :		/* input parameters */
		      [idtd] "m"(*idtd));
}

void init_interrupt(uint16_t pcpu_id)
{
	struct host_idt_descriptor *idtd = &HOST_IDTR;

	if (pcpu_id == BOOT_CPU_ID) {
		fixup_idt(idtd);
	}
	set_idt(idtd);
	init_lapic(pcpu_id);
	init_default_irqs(pcpu_id);

	firmware_init_irq();
}
