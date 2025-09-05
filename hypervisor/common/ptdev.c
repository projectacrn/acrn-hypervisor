/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hash.h>
#include <per_cpu.h>
#include <asm/guest/vm.h>
#include <softirq.h>
#include <ptdev.h>
#include <irq.h>
#include <logmsg.h>
#include <asm/vtd.h>
#include <ticks.h>

#define PTIRQ_ENTRY_HASHBITS	9U
#define PTIRQ_ENTRY_HASHSIZE	(1U << PTIRQ_ENTRY_HASHBITS)

#define PTIRQ_BITMAP_ARRAY_SIZE	INT_DIV_ROUNDUP(CONFIG_MAX_PT_IRQ_ENTRIES, 64U)
struct ptirq_remapping_info ptirq_entries[CONFIG_MAX_PT_IRQ_ENTRIES];
static uint64_t ptirq_entry_bitmaps[PTIRQ_BITMAP_ARRAY_SIZE];
spinlock_t ptdev_lock = { .head = 0U, .tail = 0U, };

/* lookup mapping info from phyical sid, hashing from sid + acrn_vm structure address (NULL) */
static struct hlist_head phys_sid_htable[PTIRQ_ENTRY_HASHSIZE];
/* lookup mapping info from virtual sid within a vm, hashing from sid + acrn_vm structure address */
static struct hlist_head virt_sid_htable[PTIRQ_ENTRY_HASHSIZE];

static inline uint16_t ptirq_alloc_entry_id(void)
{
	uint16_t id = (uint16_t)ffz64_ex(ptirq_entry_bitmaps, CONFIG_MAX_PT_IRQ_ENTRIES);

	while (id < CONFIG_MAX_PT_IRQ_ENTRIES) {
		if (!bitmap_test_and_set((id & 0x3FU), &ptirq_entry_bitmaps[id >> 6U])) {
			break;
		}
		id = (uint16_t)ffz64_ex(ptirq_entry_bitmaps, CONFIG_MAX_PT_IRQ_ENTRIES);
	}

	return (id < CONFIG_MAX_PT_IRQ_ENTRIES) ? id: INVALID_PTDEV_ENTRY_ID;
}

/*
 * get the hash key when looking up ptirq_remapping_info from virtual
 * source id in a VM, or just physical source id (vm == NULL).
 * Hashing from source id value and acrn_vm structure address can decrease
 * the probability of hash collisions as different VMs may have equal
 * virtual source ids.
 */
static inline uint64_t ptirq_hash_key(const struct acrn_vm *vm,
		const union source_id *sid)
{
	return hash64(sid->value + (uint64_t)vm, PTIRQ_ENTRY_HASHBITS);
}

/*
 * to find ptirq_remapping_info from phyical source id (vm == NULL) or
 * virtual source id in a vm.
 */
struct ptirq_remapping_info *find_ptirq_entry(uint32_t intr_type,
		const union source_id *sid, const struct acrn_vm *vm)
{
	struct hlist_node *p;
	struct hlist_head *b;
	struct ptirq_remapping_info *n, *entry = NULL;
	uint64_t key = ptirq_hash_key(vm, sid);

	if (vm == NULL) {
		b = &(phys_sid_htable[key]);

		hlist_for_each(p, b) {
			n = hlist_entry(p, struct ptirq_remapping_info, phys_link);
			if (is_entry_active(n)) {
				if ((intr_type == n->intr_type) && (sid->value == n->phys_sid.value)) {
					entry = n;
					break;
				}
			}
		}
	} else {
		b = &(virt_sid_htable[key]);
		hlist_for_each(p, b) {
			n = hlist_entry(p, struct ptirq_remapping_info, virt_link);
			if (is_entry_active(n)) {
				if ((intr_type == n->intr_type) && (sid->value == n->virt_sid.value) && (vm == n->vm)) {
					entry = n;
					break;
				}
			}
		}
	}

	return entry;
}

static void ptirq_enqueue_softirq(struct ptirq_remapping_info *entry)
{
	uint64_t rflags;

	/* enqueue request in order, SOFTIRQ_PTDEV will pickup */
	CPU_INT_ALL_DISABLE(&rflags);

	/* avoid adding recursively */
	list_del(&entry->softirq_node);
	/* TODO: assert if entry already in list */
	list_add_tail(&entry->softirq_node, &get_cpu_var(softirq_dev_entry_list));
	CPU_INT_ALL_RESTORE(rflags);
	fire_softirq(SOFTIRQ_PTDEV);
}

static void ptirq_intr_delay_callback(void *data)
{
	struct ptirq_remapping_info *entry = (struct ptirq_remapping_info *) data;

	ptirq_enqueue_softirq(entry);
}

struct ptirq_remapping_info *ptirq_dequeue_softirq(uint16_t pcpu_id)
{
	uint64_t rflags;
	struct ptirq_remapping_info *entry = NULL;

	CPU_INT_ALL_DISABLE(&rflags);

	while (!list_empty(&get_cpu_var(softirq_dev_entry_list))) {
		entry = get_first_item(&per_cpu(softirq_dev_entry_list, pcpu_id), struct ptirq_remapping_info, softirq_node);

		list_del_init(&entry->softirq_node);

		/* if Service VM, just dequeue, if User VM, check delay timer */
		if (is_service_vm(entry->vm) || timer_expired(&entry->intr_delay_timer, cpu_ticks(), NULL)) {
			break;
		} else {
			/* add it into timer list; dequeue next one */
			(void)add_timer(&entry->intr_delay_timer);
			entry = NULL;
		}
	}

	CPU_INT_ALL_RESTORE(rflags);
	return entry;
}

struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type)
{
	struct ptirq_remapping_info *entry = NULL;
	uint16_t ptirq_id = ptirq_alloc_entry_id();

	if (ptirq_id < CONFIG_MAX_PT_IRQ_ENTRIES) {
		entry = &ptirq_entries[ptirq_id];
		(void)memset((void *)entry, 0U, sizeof(struct ptirq_remapping_info));
		entry->ptdev_entry_id = ptirq_id;
		entry->intr_type = intr_type;
		entry->vm = vm;
		entry->intr_count = 0UL;
		entry->irte_idx = INVALID_IRTE_ID;

		INIT_LIST_HEAD(&entry->softirq_node);

		initialize_timer(&entry->intr_delay_timer, ptirq_intr_delay_callback, entry, 0UL, 0UL);

		entry->active = false;
	} else {
		pr_err("Alloc ptdev irq entry failed");
	}

	return entry;
}

void ptirq_release_entry(struct ptirq_remapping_info *entry)
{
	uint64_t rflags;

	CPU_INT_ALL_DISABLE(&rflags);
	list_del_init(&entry->softirq_node);
	del_timer(&entry->intr_delay_timer);
	CPU_INT_ALL_RESTORE(rflags);

	bitmap_clear((entry->ptdev_entry_id) & 0x3FU, &ptirq_entry_bitmaps[entry->ptdev_entry_id >> 6U]);

	(void)memset((void *)entry, 0U, sizeof(struct ptirq_remapping_info));
}

/* interrupt context */
static void ptirq_interrupt_handler(__unused uint32_t irq, void *data)
{
	struct ptirq_remapping_info *entry = (struct ptirq_remapping_info *) data;
	bool to_enqueue = true;

	/*
	 * "interrupt storm" detection & delay intr injection just for User VM
	 * pass-thru devices, collect its data and delay injection if needed
	 */
	if (!is_service_vm(entry->vm)) {
		entry->intr_count++;

		/* if delta > 0, set the delay TSC, dequeue to handle */
		if (entry->vm->intr_inject_delay_delta > 0UL) {

			/* if the timer started (entry is in timer-list), not need enqueue again */
			if (timer_is_started(&entry->intr_delay_timer)) {
				to_enqueue = false;
			} else {
				update_timer(&entry->intr_delay_timer,
					     cpu_ticks() + entry->vm->intr_inject_delay_delta, 0UL);
			}
		} else {
			update_timer(&entry->intr_delay_timer, 0UL, 0UL);
		}
	}

	if (to_enqueue) {
		ptirq_enqueue_softirq(entry);
	}
}

/* active intr with irq registering */
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq)
{
	int32_t ret = 0;
	uint32_t irq = IRQ_INVALID;
	uint64_t key;

	if ((entry->intr_type == PTDEV_INTR_INTX) || !is_pi_capable(entry->vm)) {
		/* register and allocate host vector/irq */
		ret = request_irq(phys_irq, ptirq_interrupt_handler, (void *)entry, IRQF_PT);
		if (ret >=0) {
			irq = (uint32_t)ret;
		} else {
			pr_err("request irq failed, please check!, phys-irq=%d", phys_irq);
		}
	}

	if (ret >=0) {
		entry->allocated_pirq = irq;
		entry->active = true;

		key = ptirq_hash_key(NULL, &(entry->phys_sid));
		hlist_add_head(&entry->phys_link, &(phys_sid_htable[key]));
		key = ptirq_hash_key(entry->vm, &(entry->virt_sid));
		hlist_add_head(&entry->virt_link, &(virt_sid_htable[key]));
	}

	return ret;
}

void ptirq_deactivate_entry(struct ptirq_remapping_info *entry)
{
	hlist_del(&entry->phys_link);
	hlist_del(&entry->virt_link);
	entry->active = false;
	if (entry->allocated_pirq != IRQ_INVALID) {
		free_irq(entry->allocated_pirq);
	}
}

void ptdev_init(void)
{
	if (get_pcpu_id() == BSP_CPU_ID) {
		register_softirq(SOFTIRQ_PTDEV, ptirq_softirq);
	}
	INIT_LIST_HEAD(&get_cpu_var(softirq_dev_entry_list));
}

void ptdev_release_all_entries(const struct acrn_vm *vm)
{
	struct ptirq_remapping_info *entry;
	uint16_t idx;

	/* VM already down */
	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if ((entry->vm == vm) && is_entry_active(entry)) {
			spinlock_obtain(&ptdev_lock);
			if (entry->release_cb != NULL) {
				entry->release_cb(entry);
			}
			ptirq_deactivate_entry(entry);
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
		if (!is_entry_active(entry) || (entry->allocated_pirq == IRQ_INVALID)) {
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
