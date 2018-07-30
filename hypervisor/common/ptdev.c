/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <softirq.h>
#include <ptdev.h>

/* SOFTIRQ_PTDEV list for all CPUs */
struct list_head softirq_dev_entry_list;
/* passthrough device link */
struct list_head ptdev_list;
spinlock_t ptdev_lock;

/* invalid_entry for error return */
struct ptdev_remapping_info invalid_entry = {
	.type = PTDEV_INTR_INV,
};

/*
 * entry could both be in ptdev_list and softirq_dev_entry_list.
 * When release entry, we need make sure entry deleted from both
 * lists. We have to require two locks and the lock sequence is:
 *   ptdev_lock
 *     softirq_dev_lock
 */
spinlock_t softirq_dev_lock;

static void ptdev_enqueue_softirq(struct ptdev_remapping_info *entry)
{
	spinlock_rflags;
	/* enqueue request in order, SOFTIRQ_PTDEV will pickup */
	spinlock_irqsave_obtain(&softirq_dev_lock);

	/* avoid adding recursively */
	list_del(&entry->softirq_node);
	/* TODO: assert if entry already in list */
	list_add_tail(&entry->softirq_node,
			&softirq_dev_entry_list);
	spinlock_irqrestore_release(&softirq_dev_lock);
	fire_softirq(SOFTIRQ_PTDEV);
}

struct ptdev_remapping_info*
ptdev_dequeue_softirq(void)
{
	struct ptdev_remapping_info *entry = NULL;

	spinlock_rflags;
	spinlock_irqsave_obtain(&softirq_dev_lock);

	if (!list_empty(&softirq_dev_entry_list)) {
		entry = get_first_item(&softirq_dev_entry_list,
			struct ptdev_remapping_info, softirq_node);
		list_del_init(&entry->softirq_node);
	}

	spinlock_irqrestore_release(&softirq_dev_lock);
	return entry;
}

/* require ptdev_lock protect */
struct ptdev_remapping_info *
alloc_entry(struct vm *vm, enum ptdev_intr_type type)
{
	struct ptdev_remapping_info *entry;

	/* allocate */
	entry = calloc(1, sizeof(*entry));
	ASSERT(entry != NULL, "alloc memory failed");
	entry->type = type;
	entry->vm = vm;

	INIT_LIST_HEAD(&entry->softirq_node);
	INIT_LIST_HEAD(&entry->entry_node);

	atomic_clear32(&entry->active, ACTIVE_FLAG);
	list_add(&entry->entry_node, &ptdev_list);

	return entry;
}

/* require ptdev_lock protect */
void
release_entry(struct ptdev_remapping_info *entry)
{
	spinlock_rflags;

	/* remove entry from ptdev_list */
	list_del_init(&entry->entry_node);

	/*
	 * remove entry from softirq list.the ptdev_lock
	 * is required before calling release_entry.
	 */
	spinlock_irqsave_obtain(&softirq_dev_lock);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&softirq_dev_lock);

	free(entry);
}

/* require ptdev_lock protect */
static void
release_all_entries(struct vm *vm)
{
	struct ptdev_remapping_info *entry;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &ptdev_list) {
		entry = list_entry(pos, struct ptdev_remapping_info,
				entry_node);
		if (entry->vm == vm)
			release_entry(entry);
	}
}

/* interrupt context */
static int ptdev_interrupt_handler(__unused int irq, void *data)
{
	struct ptdev_remapping_info *entry =
		(struct ptdev_remapping_info *) data;

	ptdev_enqueue_softirq(entry);
	return 0;
}

/* active intr with irq registering */
void
ptdev_activate_entry(struct ptdev_remapping_info *entry, uint32_t phys_irq,
		bool lowpri)
{
	struct dev_handler_node *node;

	/* register and allocate host vector/irq */
	node = normal_register_handler(phys_irq, ptdev_interrupt_handler,
		(void *)entry, true, lowpri, "dev assign");

	ASSERT(node != NULL, "dev register failed");
	entry->node = node;

	atomic_set32(&entry->active, ACTIVE_FLAG);
}

void
ptdev_deactivate_entry(struct ptdev_remapping_info *entry)
{
	spinlock_rflags;

	atomic_clear32(&entry->active, ACTIVE_FLAG);

	unregister_handler_common(entry->node);
	entry->node = NULL;

	/* remove from softirq list if added */
	spinlock_irqsave_obtain(&softirq_dev_lock);
	list_del_init(&entry->softirq_node);
	spinlock_irqrestore_release(&softirq_dev_lock);
}

void ptdev_init(void)
{
	if (get_cpu_id() > 0)
		return;

	INIT_LIST_HEAD(&ptdev_list);
	spinlock_init(&ptdev_lock);
	INIT_LIST_HEAD(&softirq_dev_entry_list);
	spinlock_init(&softirq_dev_lock);

	register_softirq(SOFTIRQ_PTDEV, ptdev_softirq);
}

void ptdev_release_all_entries(struct vm *vm)
{
	/* VM already down */
	spinlock_obtain(&ptdev_lock);
	release_all_entries(vm);
	spinlock_release(&ptdev_lock);
}
