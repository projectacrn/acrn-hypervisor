/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <softirq.h>
#include <ptdev.h>

/* passthrough device link */
struct list_head ptdev_list;
spinlock_t ptdev_lock;

/*
 * entry could both be in ptdev_list and vm->softirq_dev_entry_list.
 * When release entry, we need make sure entry deleted from both
 * lists. We have to require two locks and the lock sequence is:
 *   ptdev_lock
 *     vm->softirq_dev_lock
 */

static void ptdev_enqueue_softirq(struct ptdev_remapping_info *entry)
{
	uint64_t rflags;

	/* enqueue request in order, SOFTIRQ_PTDEV will pickup */
	spinlock_irqsave_obtain(&entry->vm->softirq_dev_lock, &rflags);

	/* avoid adding recursively */
	list_del(&entry->softirq_node);
	/* TODO: assert if entry already in list */
	list_add_tail(&entry->softirq_node,
			&entry->vm->softirq_dev_entry_list);
	spinlock_irqrestore_release(&entry->vm->softirq_dev_lock, rflags);
	fire_softirq(SOFTIRQ_PTDEV);
}

static void ptdev_intr_delay_callback(void *data)
{
	struct ptdev_remapping_info *entry =
		(struct ptdev_remapping_info *) data;

	ptdev_enqueue_softirq(entry);
}

struct ptdev_remapping_info*
ptdev_dequeue_softirq(struct acrn_vm *vm)
{
	uint64_t rflags;
	struct ptdev_remapping_info *entry = NULL;

	spinlock_irqsave_obtain(&vm->softirq_dev_lock, &rflags);

	while (!list_empty(&vm->softirq_dev_entry_list)) {
		entry = get_first_item(&vm->softirq_dev_entry_list,
			struct ptdev_remapping_info, softirq_node);

		list_del_init(&entry->softirq_node);

		/* if vm0, just dequeue, if uos, check delay timer */
		if (is_vm0(entry->vm) ||
			timer_expired(&entry->intr_delay_timer)) {
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

/* require ptdev_lock protect */
struct ptdev_remapping_info *
alloc_entry(struct acrn_vm *vm, uint32_t intr_type)
{
	struct ptdev_remapping_info *entry;

	/* allocate */
	entry = calloc(1U, sizeof(*entry));
	ASSERT(entry != NULL, "alloc memory failed");
	entry->intr_type = intr_type;
	entry->vm = vm;

	INIT_LIST_HEAD(&entry->softirq_node);
	INIT_LIST_HEAD(&entry->entry_node);

	entry->intr_count = 0UL;
	initialize_timer(&entry->intr_delay_timer, ptdev_intr_delay_callback,
		entry, 0UL, 0, 0UL);

	atomic_clear32(&entry->active, ACTIVE_FLAG);
	list_add(&entry->entry_node, &ptdev_list);

	return entry;
}

/* require ptdev_lock protect */
void
release_entry(struct ptdev_remapping_info *entry)
{
	uint64_t rflags;

	/* remove entry from ptdev_list */
	list_del_init(&entry->entry_node);

	/*
	 * remove entry from softirq list.the ptdev_lock
	 * is required before calling release_entry.
	 */
	spinlock_irqsave_obtain(&entry->vm->softirq_dev_lock, &rflags);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&entry->vm->softirq_dev_lock, rflags);

	free(entry);
}

/* require ptdev_lock protect */
static void
release_all_entries(const struct acrn_vm *vm)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if (entry->vm == vm) {
			release_entry(entry);
		}
	}
}

/* interrupt context */
static void ptdev_interrupt_handler(__unused uint32_t irq, void *data)
{
	struct ptdev_remapping_info *entry =
		(struct ptdev_remapping_info *) data;

	/*
	 * "interrupt storm" detection & delay intr injection just for UOS
	 * pass-thru devices, collect its data and delay injection if needed
	 */
	if (!is_vm0(entry->vm)) {
		entry->intr_count++;

		/* if delta > 0, set the delay TSC, dequeue to handle */
		if (entry->vm->intr_inject_delay_delta > 0UL) {
			entry->intr_delay_timer.fire_tsc = rdtsc() +
				entry->vm->intr_inject_delay_delta;
		} else {
			entry->intr_delay_timer.fire_tsc = 0UL;
		}
	}

	ptdev_enqueue_softirq(entry);
}

/* active intr with irq registering */
void
ptdev_activate_entry(struct ptdev_remapping_info *entry, uint32_t phys_irq)
{
	int32_t retval;

	/* register and allocate host vector/irq */
	retval = request_irq(phys_irq, ptdev_interrupt_handler,
		             (void *)entry, IRQF_PT);

	ASSERT(retval >= 0, "dev register failed");
	entry->allocated_pirq = (uint32_t)retval;

	atomic_set32(&entry->active, ACTIVE_FLAG);
}

void
ptdev_deactivate_entry(struct ptdev_remapping_info *entry)
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

	INIT_LIST_HEAD(&ptdev_list);
	spinlock_init(&ptdev_lock);

	register_softirq(SOFTIRQ_PTDEV, ptdev_softirq);
}

void ptdev_release_all_entries(const struct acrn_vm *vm)
{
	/* VM already down */
	spinlock_obtain(&ptdev_lock);
	release_all_entries(vm);
	spinlock_release(&ptdev_lock);
}

uint32_t get_vm_ptdev_intr_data(const struct acrn_vm *target_vm, uint64_t *buffer,
	uint32_t buffer_cnt)
{
	uint32_t index = 0U;
	struct ptdev_remapping_info *entry;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if (entry->vm == target_vm) {
			buffer[index] = entry->allocated_pirq;
			buffer[index + 1U] = entry->intr_count;

			index += 2U;
			if (index >= buffer_cnt) {
				break;
			}
		}
	}

	return index;
}
