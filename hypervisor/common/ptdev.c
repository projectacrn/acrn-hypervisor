/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <softirq.h>
#include <ptdev.h>

#define PTIRQ_BITMAP_ARRAY_SIZE	INT_DIV_ROUNDUP(CONFIG_MAX_PT_IRQ_ENTRIES, 64U)
struct ptirq_remapping_info ptirq_entries[CONFIG_MAX_PT_IRQ_ENTRIES];
static uint64_t ptirq_entry_bitmaps[PTIRQ_BITMAP_ARRAY_SIZE];
spinlock_t ptdev_lock;

bool is_entry_active(const struct ptirq_remapping_info *entry)
{
	return atomic_load32(&entry->active) == ACTIVE_FLAG;
}

static inline uint16_t ptirq_alloc_entry_id(void)
{
	uint16_t id = (uint16_t)ffz64_ex(ptirq_entry_bitmaps, CONFIG_MAX_PT_IRQ_ENTRIES);

	while (id < CONFIG_MAX_PT_IRQ_ENTRIES) {
		if (!bitmap_test_and_set_lock((id & 0x3FU), &ptirq_entry_bitmaps[id >> 6U])) {
			return id;
		}
		id = (uint16_t)ffz64_ex(ptirq_entry_bitmaps, CONFIG_MAX_PT_IRQ_ENTRIES);
	}

	return INVALID_PTDEV_ENTRY_ID;
}

static void ptirq_enqueue_softirq(struct ptirq_remapping_info *entry)
{
	uint64_t rflags;

	/* enqueue request in order, SOFTIRQ_PTDEV will pickup */
	spinlock_irqsave_obtain(&entry->vm->softirq_dev_lock, &rflags);

	/* avoid adding recursively */
	list_del(&entry->softirq_node);
	/* TODO: assert if entry already in list */
	list_add_tail(&entry->softirq_node, &entry->vm->softirq_dev_entry_list);
	spinlock_irqrestore_release(&entry->vm->softirq_dev_lock, rflags);
	fire_softirq(SOFTIRQ_PTDEV);
}

static void ptirq_intr_delay_callback(void *data)
{
	struct ptirq_remapping_info *entry = (struct ptirq_remapping_info *) data;

	ptirq_enqueue_softirq(entry);
}

struct ptirq_remapping_info *ptirq_dequeue_softirq(struct acrn_vm *vm)
{
	uint64_t rflags;
	struct ptirq_remapping_info *entry = NULL;

	spinlock_irqsave_obtain(&vm->softirq_dev_lock, &rflags);

	while (!list_empty(&vm->softirq_dev_entry_list)) {
		entry = get_first_item(&vm->softirq_dev_entry_list, struct ptirq_remapping_info, softirq_node);

		list_del_init(&entry->softirq_node);

		/* if vm0, just dequeue, if uos, check delay timer */
		if (is_vm0(entry->vm) || timer_expired(&entry->intr_delay_timer)) {
			break;
		} else {
			/* add it into timer list; dequeue next one */
			(void)add_timer(&entry->intr_delay_timer);
			entry = NULL;
		}
	}

	spinlock_irqrestore_release(&vm->softirq_dev_lock, rflags);
	return entry;
}

struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type)
{
	struct ptirq_remapping_info *entry;
	uint16_t ptirq_id = ptirq_alloc_entry_id();

	if (ptirq_id >= CONFIG_MAX_PT_IRQ_ENTRIES) {
		pr_err("Alloc ptdev irq entry failed");
		return NULL;
	}

	entry = &ptirq_entries[ptirq_id];
	(void)memset((void *)entry, 0U, sizeof(struct ptirq_remapping_info));
	entry->ptdev_entry_id = ptirq_id;
	entry->intr_type = intr_type;
	entry->vm = vm;
	entry->intr_count = 0UL;

	INIT_LIST_HEAD(&entry->softirq_node);

	initialize_timer(&entry->intr_delay_timer, ptirq_intr_delay_callback, entry, 0UL, 0, 0UL);

	atomic_clear32(&entry->active, ACTIVE_FLAG);

	return entry;
}

void ptirq_release_entry(struct ptirq_remapping_info *entry)
{
	uint64_t rflags;

	/*
	 * remove entry from softirq list
	 * is required before calling ptirq_release_entry.
	 */
	spinlock_irqsave_obtain(&entry->vm->softirq_dev_lock, &rflags);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&entry->vm->softirq_dev_lock, rflags);
	atomic_clear32(&entry->active, ACTIVE_FLAG);
	bitmap_clear_nolock((entry->ptdev_entry_id) & 0x3FU,
		&ptirq_entry_bitmaps[((entry->ptdev_entry_id) & 0x3FU) >> 6U]);
}

/* interrupt context */
static void ptirq_interrupt_handler(__unused uint32_t irq, void *data)
{
	struct ptirq_remapping_info *entry = (struct ptirq_remapping_info *) data;
	bool to_enqueue = true;

	/*
	 * "interrupt storm" detection & delay intr injection just for UOS
	 * pass-thru devices, collect its data and delay injection if needed
	 */
	if (!is_vm0(entry->vm)) {
		entry->intr_count++;

		/* if delta > 0, set the delay TSC, dequeue to handle */
		if (entry->vm->intr_inject_delay_delta > 0UL) {

			/* if the timer started (entry is in timer-list), not need enqueue again */
			if (timer_is_started(&entry->intr_delay_timer)) {
				to_enqueue = false;
			} else {
				entry->intr_delay_timer.fire_tsc = rdtsc() + entry->vm->intr_inject_delay_delta;
			}
		} else {
			entry->intr_delay_timer.fire_tsc = 0UL;
		}
	}

	if (to_enqueue) {
		ptirq_enqueue_softirq(entry);
	}
}

/* active intr with irq registering */
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq)
{
	int32_t retval;

	/* register and allocate host vector/irq */
	retval = request_irq(phys_irq, ptirq_interrupt_handler, (void *)entry, IRQF_PT);

	if (retval < 0) {
		pr_err("request irq failed, please check!, phys-irq=%d", phys_irq);
	} else {
		entry->allocated_pirq = (uint32_t)retval;
		atomic_set32(&entry->active, ACTIVE_FLAG);
	}

	return retval;
}

void ptirq_deactivate_entry(struct ptirq_remapping_info *entry)
{
	uint64_t rflags;

	atomic_clear32(&entry->active, ACTIVE_FLAG);

	free_irq(entry->allocated_pirq);
	entry->allocated_pirq = IRQ_INVALID;

	/* remove from softirq list if added */
	spinlock_irqsave_obtain(&entry->vm->softirq_dev_lock, &rflags);
	list_del_init(&entry->softirq_node);
	del_timer(&entry->intr_delay_timer);
	spinlock_irqrestore_release(&entry->vm->softirq_dev_lock, rflags);
}

void ptdev_init(void)
{
	if (get_cpu_id() != BOOT_CPU_ID) {
		return;
	}

	spinlock_init(&ptdev_lock);
	register_softirq(SOFTIRQ_PTDEV, ptirq_softirq);
}

void ptdev_release_all_entries(const struct acrn_vm *vm)
{
	struct ptirq_remapping_info *entry;
	uint16_t idx;

	/* VM already down */
	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if (entry->vm == vm) {
			spinlock_obtain(&ptdev_lock);
			ptirq_release_entry(entry);
			spinlock_release(&ptdev_lock);
		}
	}

}

uint32_t ptirq_get_intr_data(const struct acrn_vm *target_vm, uint64_t *buffer, uint32_t buffer_cnt)
{
	uint32_t index = 0U;
	uint16_t i;
	struct ptirq_remapping_info *entry;

	for (i = 0U; i < CONFIG_MAX_PT_IRQ_ENTRIES; i++) {
		entry = &ptirq_entries[i];
		if (!is_entry_active(entry)) {
			continue;
		}
		if (entry->vm == target_vm) {
			buffer[index] = entry->allocated_pirq;
			buffer[index + 1U] = entry->intr_count;

			index += 2U;
			if (index > (buffer_cnt - 2U)) {
				break;
			}
		}
	}

	return index;
}
