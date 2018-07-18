/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTDEV_H
#define PTDEV_H

#define ACTIVE_FLAG 0x1U /* any non zero should be okay */

enum ptdev_intr_type {
	PTDEV_INTR_MSI,
	PTDEV_INTR_INTX,
	PTDEV_INTR_INV,
};

enum ptdev_vpin_source {
	PTDEV_VPIN_IOAPIC,
	PTDEV_VPIN_PIC,
};

/* entry per guest virt vector */
struct ptdev_msi_info {
	uint32_t vmsi_addr; /* virt msi_addr */
	uint32_t vmsi_data; /* virt msi_data */
	uint16_t vmsi_ctl; /* virt msi_ctl */
	uint32_t pmsi_addr; /* phys msi_addr */
	uint32_t pmsi_data; /* phys msi_data */
	int msix;	/* 0-MSI, 1-MSIX */
	int msix_entry_index; /* MSI: 0, MSIX: index of vector table*/
	uint32_t virt_vector;
	uint32_t phys_vector;
};

/* entry per guest vioapic pin */
struct ptdev_intx_info {
	enum ptdev_vpin_source vpin_src;
	uint8_t virt_pin;
	uint8_t phys_pin;
};

/* entry per each allocated irq/vector
 * it represents a pass-thru device's remapping data entry which collecting
 * information related with its vm and msi/intx mapping & interaction nodes
 * with interrupt handler and softirq.
 */
struct ptdev_remapping_info {
	struct vm *vm;
	uint16_t virt_bdf;	/* PCI bus:slot.func*/
	uint16_t phys_bdf;	/* PCI bus:slot.func*/
	uint32_t active;	/* 1=active, 0=inactive and to free*/
	enum ptdev_intr_type type;
	struct dev_handler_node *node;
	struct list_head softirq_node;
	struct list_head entry_node;

	union {
		struct ptdev_msi_info msi;
		struct ptdev_intx_info intx;
	} ptdev_intr_info;
};

extern struct list_head softirq_dev_entry_list;
extern struct list_head ptdev_list;
extern spinlock_t ptdev_lock;
extern struct ptdev_remapping_info invalid_entry;
extern spinlock_t softirq_dev_lock;

void ptdev_softirq(__unused uint16_t cpu);
void ptdev_init(void);
void ptdev_release_all_entries(struct vm *vm);

struct ptdev_remapping_info *ptdev_dequeue_softirq(void);
struct ptdev_remapping_info *alloc_entry(struct vm *vm,
		enum ptdev_intr_type type);
void release_entry(struct ptdev_remapping_info *entry);
void ptdev_activate_entry(
		struct ptdev_remapping_info *entry,
		uint32_t phys_irq, bool lowpri);
void ptdev_deactivate_entry(struct ptdev_remapping_info *entry);

#ifdef HV_DEBUG
void get_ptdev_info(char *str, int str_max);
#endif /* HV_DEBUG */

#endif /* PTDEV_H */
