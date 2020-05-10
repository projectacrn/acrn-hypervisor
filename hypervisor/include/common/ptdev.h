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


enum intx_ctlr {
	INTX_CTLR_IOAPIC	= 0U,
	INTX_CTLR_PIC
};

#define PTDEV_INTR_MSI		(1U << 0U)
#define PTDEV_INTR_INTX		(1U << 1U)

#define INVALID_PTDEV_ENTRY_ID 0xffffU

#define DEFINE_MSI_SID(name, a, b)	\
union source_id (name) = {.msi_id = {.bdf = (a), .entry_nr = (b)} }

#define DEFINE_INTX_SID(name, a, b)	\
union source_id (name) = {.intx_id = {.gsi = (a), .ctlr = (b)} }

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
	/*
	 * ctlr indicates if the source of interrupt is IO-APIC or PIC
	 * pin indicates the pin number of interrupt controller determined by ctlr
	 */
	struct {
		enum intx_ctlr ctlr;
		uint32_t gsi;
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

struct msi_info {
	union msi_addr_reg addr;
	union msi_data_reg data;
};

struct ptirq_remapping_info;
typedef void (*ptirq_arch_release_fn_t)(const struct ptirq_remapping_info *entry);

/* entry per each allocated irq/vector
 * it represents a pass-thru device's remapping data entry which collecting
 * information related with its vm and msi/intx mapping & interaction nodes
 * with interrupt handler and softirq.
 */
struct ptirq_remapping_info {
	struct hlist_node phys_link;
	struct hlist_node virt_link;
	uint16_t ptdev_entry_id;
	uint32_t intr_type;
	union source_id phys_sid;
	union source_id virt_sid;
	struct acrn_vm *vm;
	bool active;	/* true=active, false=inactive*/
	uint32_t allocated_pirq;
	uint32_t polarity; /* 0=active high, 1=active low*/
	struct list_head softirq_node;
	struct msi_info vmsi;
	struct msi_info pmsi;
	uint16_t irte_idx;

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

/**
 * @file ptdev.h
 *
 * @brief public APIs for ptdev
 */

/**
 * @brief ptdev
 *
 * @addtogroup acrn_passthrough
 * @{
 */


/*
 * @brief Find a ptdev entry by sid
 *
 * param[in] intr_type interrupt type of the ptirq entry
 * param[in] sid source id of the ptirq entry
 * param[in] vm vm pointer of the ptirq entry if find the ptdev entry by virtual sid
 *
 * @retval NULL when \p when no ptirq entry match the sid
 * @retval ptirq entry when \p there is available ptirq entry match the sid
 *
 * @pre: vm must be NULL when lookup by physical sid, otherwise,
 * vm must not be NULL when lookup by virtual sid.
 */
struct ptirq_remapping_info *find_ptirq_entry(uint32_t intr_type,
		const union source_id *sid, const struct acrn_vm *vm);

/**
 * @brief Handler of softirq for passthrough device.
 *
 * When hypervisor receive a physical interrupt from passthrough device, it
 * will enqueue a ptirq entry and raise softirq SOFTIRQ_PTDEV. This function
 * is the handler of the softirq, it handles the interrupt and injects the
 * virtual into VM.
 * The handler is registered by calling @ref ptdev_init during hypervisor
 * initialization.
 *
 * @param[in]    pcpu_id physical cpu id of the soft irq
 *
 */
void ptirq_softirq(uint16_t pcpu_id);
/**
 * @brief Passthrough device global data structure initialization.
 *
 * During the hypervisor cpu initialization stage, this function:
 * - init global spinlock for ptdev (on BSP)
 * - register SOFTIRQ_PTDEV handler (on BSP)
 * - init the softirq entry list for each CPU
 *
 */
void ptdev_init(void);
/**
 * @brief Deactivate and release all ptirq entries for a VM.
 *
 * This function deactivates and releases all ptirq entries for a VM. The function
 * should only be called after the VM is already down.
 *
 * @param[in]    vm acrn_vm on which the ptirq entries will be released
 *
 * @pre VM is already down
 *
 */
void ptdev_release_all_entries(const struct acrn_vm *vm);

/**
 * @brief Dequeue an entry from per cpu ptdev softirq queue.
 *
 * Dequeue an entry from the ptdev softirq queue on the specific physical cpu.
 *
 * @param[in]    pcpu_id physical cpu id
 *
 * @retval NULL when \p when the queue is empty
 * @retval !NULL when \p there is available ptirq_remapping_info entry in the queue
 *
 */
struct ptirq_remapping_info *ptirq_dequeue_softirq(uint16_t pcpu_id);
/**
 * @brief Allocate a ptirq_remapping_info entry.
 *
 * Allocate a ptirq_remapping_info entry for hypervisor to store the remapping information.
 * The total number of the entries is statically defined as CONFIG_MAX_PT_IRQ_ENTRIES.
 * Appropriate number should be configured on different platforms.
 *
 * @param[in]    vm acrn_vm that the entry allocated for.
 * @param[in]    intr_type interrupt type: PTDEV_INTR_MSI or PTDEV_INTR_INTX
 *
 * @retval NULL when \p the number of entries allocated is CONFIG_MAX_PT_IRQ_ENTRIES
 * @retval !NULL when \p the number of entries allocated is less than CONFIG_MAX_PT_IRQ_ENTRIES
 *
 */
struct ptirq_remapping_info *ptirq_alloc_entry(struct acrn_vm *vm, uint32_t intr_type);
/**
 * @brief Release a ptirq_remapping_info entry.
 *
 * @param[in]    entry the ptirq_remapping_info entry to release.
 *
 */
void ptirq_release_entry(struct ptirq_remapping_info *entry);
/**
 * @brief Activate a irq for the associated passthrough device.
 *
 * After activating the ptirq entry, the physical interrupt irq of passthrough device will be handled
 * by the handler  ptirq_interrupt_handler.
 *
 * @param[in]    entry the ptirq_remapping_info entry that will be associated with the physical irq.
 * @param[in]    phys_irq physical interrupt irq for the entry
 *
 * @retval success when \p return value >=0
 * @retval success when \p return <0
 *
 */
int32_t ptirq_activate_entry(struct ptirq_remapping_info *entry, uint32_t phys_irq);
/**
 * @brief De-activate a irq for the associated passthrough device.
 *
 * @param[in]    entry the ptirq_remapping_info entry that will be de-activated.
 *
 */
void ptirq_deactivate_entry(struct ptirq_remapping_info *entry);
/**
 * @brief Get the interrupt information and store to the buffer provided.
 *
 * @param[in]    target_vm the VM to get the interrupt information.
 * @param[out]    buffer the buffer to interrupt information stored to.
 * @param[in]    buffer_cnt the size of the buffer.
 *
 * @retval the actual size the buffer filled with the interrupt information
 *
 */
uint32_t ptirq_get_intr_data(const struct acrn_vm *target_vm, uint64_t *buffer, uint32_t buffer_cnt);

/**
  * @}
  */

#endif /* PTDEV_H */
