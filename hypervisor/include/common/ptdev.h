/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTDEV_H
#define PTDEV_H

#define ACTIVE_FLAG 0x1U /* any non zero should be okay */

#define PTDEV_INTR_MSI		(1U << 0U)
#define PTDEV_INTR_INTX		(1U << 1U)

#define INVALID_PTDEV_ENTRY_ID 0xffffU

enum ptirq_vpin_source {
	PTDEV_VPIN_IOAPIC,
	PTDEV_VPIN_PIC,
};

#define DEFINE_MSI_SID(name, a, b)	\
union source_id (name) = {.msi_id = {.bdf = (a), .entry_nr = (b)} }

#define DEFINE_IOAPIC_SID(name, a, b)	\
union source_id (name) = {.intx_id = {.pin = (a), .src = (b)} }

union source_id {
	uint32_t value;
	struct {
		uint16_t bdf;
		uint16_t entry_nr;
	} msi_id;
	struct {
		uint8_t pin;
		uint8_t src;
		uint16_t reserved;
	} intx_id;
};

/* entry per guest virt vector */
struct ptirq_msi_info {
	uint64_t vmsi_addr; /* virt msi_addr */
	uint32_t vmsi_data; /* virt msi_data */
	uint64_t pmsi_addr; /* phys msi_addr */
	uint32_t pmsi_data; /* phys msi_data */
	int is_msix;	/* 0-MSI, 1-MSIX */
};

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
	uint32_t active;	/* 1=active, 0=inactive and to free*/
	uint32_t allocated_pirq;
	uint32_t polarity; /* 0=active high, 1=active low*/
	struct list_head softirq_node;
	struct ptirq_msi_info msi;

	uint64_t intr_count;
	struct hv_timer intr_delay_timer; /* used for delay intr injection */
};

extern struct ptirq_remapping_info ptirq_entries[];
extern spinlock_t ptdev_lock;

bool is_entry_active(const struct ptirq_remapping_info *entry);
void ptirq_softirq(uint16_t pcpu_id);
void ptdev_init(void);
void ptdev_release_all_entries(const struct acrn_vm *vm);

struct ptirq_remapping_info *ptirq_dequeue_softirq(struct acrn_vm *vm);
struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type);
void ptirq_release_entry(struct ptirq_remapping_info *entry);
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq);
void ptirq_deactivate_entry(struct ptirq_remapping_info *entry);

uint32_t ptirq_get_intr_data(const struct acrn_vm *target_vm, uint64_t *buffer, uint32_t buffer_cnt);

#endif /* PTDEV_H */
