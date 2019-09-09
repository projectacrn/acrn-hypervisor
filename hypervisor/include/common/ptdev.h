/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTDEV_H
#define PTDEV_H
#include <list.h>
#include <spinlock.h>
#include <timer.h>

#define PTDEV_INTR_MSI		(1U << 0U)
#define PTDEV_INTR_INTX		(1U << 1U)

#define INVALID_PTDEV_ENTRY_ID 0xffffU

#define PTDEV_VPIN_IOAPIC	0x0U
#define	PTDEV_VPIN_PIC		0x1U

#define DEFINE_MSI_SID(name, a, b)	\
union source_id (name) = {.msi_id = {.bdf = (a), .entry_nr = (b)} }

#define DEFINE_IOAPIC_SID(name, a, b)	\
union source_id (name) = {.intx_id = {.pin = (a), .src = (b)} }

union irte_index {
	uint16_t index;
	struct {
		uint16_t index_low:15;
		uint16_t index_high:1;
	} bits __packed;
};

union source_id {
	uint64_t value;
	struct {
		uint16_t bdf;
		uint16_t entry_nr;
		uint32_t reserved;
	} msi_id;
	struct {
		uint32_t pin;
		uint32_t src;
	} intx_id;
};

/*
 * Macros for bits in union msi_addr_reg
 */

#define	MSI_ADDR_BASE			0xfeeUL	/* Base address for MSI messages */
#define	MSI_ADDR_RH			0x1U	/* Redirection Hint */
#define	MSI_ADDR_DESTMODE_LOGICAL	0x1U	/* Destination Mode: Logical*/
#define	MSI_ADDR_DESTMODE_PHYS		0x0U	/* Destination Mode: Physical*/

union msi_addr_reg {
	uint64_t full;
	struct {
		uint32_t rsvd_1:2;
		uint32_t dest_mode:1;
		uint32_t rh:1;
		uint32_t rsvd_2:8;
		uint32_t dest_field:8;
		uint32_t addr_base:12;
		uint32_t hi_32;
	} bits __packed;
	struct {
		uint32_t rsvd_1:2;
		uint32_t intr_index_high:1;
		uint32_t shv:1;
		uint32_t intr_format:1;
		uint32_t intr_index_low:15;
		uint32_t constant:12;
		uint32_t hi_32;
	} ir_bits __packed;

};

/*
 * Macros for bits in union msi_data_reg
 */

#define MSI_DATA_DELMODE_FIXED		0x0U	/* Delivery Mode: Fixed */
#define MSI_DATA_DELMODE_LOPRI		0x1U	/* Delivery Mode: Low Priority */
#define MSI_DATA_TRGRMODE_EDGE		0x0U	/* Trigger Mode: Edge */
#define MSI_DATA_TRGRMODE_LEVEL		0x1U	/* Trigger Mode: Level */

union msi_data_reg {
	uint32_t full;
	struct {
		uint32_t vector:8;
		uint32_t delivery_mode:3;
		uint32_t rsvd_1:3;
		uint32_t level:1;
		uint32_t trigger_mode:1;
		uint32_t rsvd_2:16;
	} bits __packed;
};

/* entry per guest virt vector */
struct ptirq_msi_info {
	union msi_addr_reg vmsi_addr; /* virt msi_addr */
	union msi_data_reg vmsi_data; /* virt msi_data */
	union msi_addr_reg pmsi_addr; /* phys msi_addr */
	union msi_data_reg pmsi_data; /* phys msi_data */
};

struct ptirq_remapping_info;
typedef void (*ptirq_arch_release_fn_t)(const struct ptirq_remapping_info *entry);

/* entry per each allocated irq/vector
 * it represents a pass-thru device's remapping data entry which collecting
 * information related with its vm and msi/intx mapping & interaction nodes
 * with interrupt handler and softirq.
 */
struct ptirq_remapping_info {
	uint16_t ptdev_entry_id;
	uint32_t intr_type;
	union source_id phys_sid;
	union source_id virt_sid;
	struct acrn_vm *vm;
	bool active;	/* true=active, false=inactive*/
	uint32_t allocated_pirq;
	uint32_t polarity; /* 0=active high, 1=active low*/
	struct list_head softirq_node;
	struct ptirq_msi_info msi;

	uint64_t intr_count;
	struct hv_timer intr_delay_timer; /* used for delay intr injection */
	ptirq_arch_release_fn_t release_cb;
};

static inline bool is_entry_active(const struct ptirq_remapping_info *entry)
{
	return entry->active;
}

extern struct ptirq_remapping_info ptirq_entries[CONFIG_MAX_PT_IRQ_ENTRIES];
extern spinlock_t ptdev_lock;

void ptirq_softirq(uint16_t pcpu_id);
void ptdev_init(void);
void ptdev_release_all_entries(const struct acrn_vm *vm);

struct ptirq_remapping_info *ptirq_dequeue_softirq(uint16_t pcpu_id);
struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type);
void ptirq_release_entry(struct ptirq_remapping_info *entry);
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq);
void ptirq_deactivate_entry(struct ptirq_remapping_info *entry);

uint32_t ptirq_get_intr_data(const struct acrn_vm *target_vm, uint64_t *buffer, uint32_t buffer_cnt);

#endif /* PTDEV_H */
