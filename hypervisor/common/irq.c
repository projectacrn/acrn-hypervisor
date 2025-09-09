/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <asm/lib/bits.h>
#include <irq.h>
#include <common/softirq.h>
#include <asm/irq.h>
#include <per_cpu.h>

static spinlock_t irq_alloc_spinlock = { .head = 0U, .tail = 0U, };

uint64_t irq_alloc_bitmap[IRQ_ALLOC_BITMAP_SIZE];
struct irq_desc irq_desc_array[NR_IRQS];
static uint64_t irq_rsvd_bitmap[IRQ_ALLOC_BITMAP_SIZE];

/*
 * alloc an free irq if req_irq is IRQ_INVALID, or else set assigned
 * return: irq num on success, IRQ_INVALID on failure
 */
static uint32_t alloc_irq_num(uint32_t req_irq, bool reserve)
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
			if (reserve) {
				bitmap_set_nolock((uint16_t)(irq & 0x3FU),
						irq_rsvd_bitmap + (irq >> 6U));
			}
		}
		spinlock_irqrestore_release(&irq_alloc_spinlock, rflags);
		ret = irq;
	}
	return ret;
}

uint32_t reserve_irq_num(uint32_t irq)
{
	return alloc_irq_num(irq, true);
}

/*
 * @pre: irq is not in irq_static_mappings
 * free irq num allocated via alloc_irq_num()
 */
static void free_irq_num(uint32_t irq)
{
	uint64_t rflags;

	if (irq < NR_IRQS) {
		spinlock_irqsave_obtain(&irq_alloc_spinlock, &rflags);

		if (bitmap_test((uint16_t)(irq & 0x3FU),
			irq_rsvd_bitmap + (irq >> 6U)) == false) {
			bitmap_clear_nolock((uint16_t)(irq & 0x3FU),
					irq_alloc_bitmap + (irq >> 6U));
		}
		spinlock_irqrestore_release(&irq_alloc_spinlock, rflags);
	}
}

void free_irq(uint32_t irq)
{
	uint64_t rflags;
	struct irq_desc *desc;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];

		spinlock_irqsave_obtain(&desc->lock, &rflags);
		desc->action = NULL;
		desc->priv_data = NULL;
		desc->flags = IRQF_NONE;
		spinlock_irqrestore_release(&desc->lock, rflags);

		free_irq_arch(irq);
		free_irq_num(irq);
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
	uint32_t irq;
	uint64_t rflags;
	int32_t ret;

	irq = alloc_irq_num(req_irq, false);
	if (irq == IRQ_INVALID) {
		pr_err("[%s] invalid irq num", __func__);
		ret = -EINVAL;
	} else {
		if (!request_irq_arch(irq)) {
			pr_err("[%s] failed to alloc vector for irq %u",
				__func__, irq);
			free_irq_num(irq);
			ret = -EINVAL;
		} else {
			desc = &irq_desc_array[irq];
			if (desc->action == NULL) {
				spinlock_irqsave_obtain(&desc->lock, &rflags);
				desc->flags = flags;
				desc->priv_data = priv_data;
				desc->action = action_fn;
				spinlock_irqrestore_release(&desc->lock, rflags);
				ret = (int32_t)irq;
			} else {
				ret = -EBUSY;
				pr_err("%s: request irq(%u) failed, already requested",
				       __func__, irq);
			}
		}
	}

	return ret;
}

void set_irq_trigger_mode(uint32_t irq, bool is_level_triggered)
{
	uint64_t rflags;
	struct irq_desc *desc;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];
		spinlock_irqsave_obtain(&desc->lock, &rflags);
		if (is_level_triggered) {
			desc->flags |= IRQF_LEVEL;
		} else {
			desc->flags &= ~IRQF_LEVEL;
		}
		spinlock_irqrestore_release(&desc->lock, rflags);
	}
}

static inline void handle_irq(const struct irq_desc *desc)
{
	irq_action_t action = desc->action;

	pre_irq_arch(desc);

	if (action != NULL) {
		action(desc->irq, desc->priv_data);
	}

	post_irq_arch(desc);
}

void do_irq(const uint32_t irq)
{
	struct irq_desc *desc;

	if (irq < NR_IRQS) {
		desc = &irq_desc_array[irq];
		per_cpu(irq_count, get_pcpu_id())[irq]++;

		/* XXX irq_alloc_bitmap is used lockless here */
		if (bitmap_test((uint16_t)(irq & 0x3FU), irq_alloc_bitmap + (irq >> 6U))) {
			handle_irq(desc);
		}
	}

	do_softirq();
}

static void init_irq_descs(void)
{
	uint32_t i;

	for (i = 0U; i < NR_IRQS; i++) {
		struct irq_desc *desc = &irq_desc_array[i];
		desc->irq = i;
		spinlock_init(&desc->lock);
	}

	init_irq_descs_arch(irq_desc_array);
}

void init_interrupt(uint16_t pcpu_id)
{
	init_interrupt_arch(pcpu_id);

	if (pcpu_id == BSP_CPU_ID) {
		init_irq_descs();
		setup_irqs_arch();
		init_softirq();
	}

	CPU_IRQ_ENABLE_ON_CONFIG();
}
