/*
 * Copyright (C) 2021-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_IRQ_H
#define COMMON_IRQ_H

#include <lib/util.h>
#include <spinlock.h>

/**
 * @file common/irq.h
 *
 * @brief public APIs for common IRQ handling
 */

#define NR_IRQS			256U
#define IRQ_INVALID		0xffffffffU

#define IRQ_ALLOC_BITMAP_SIZE	INT_DIV_ROUNDUP(NR_IRQS, 64U)

#define IRQF_NONE	(0U)
#define IRQF_LEVEL	(1U << 1U)	/* 1: level trigger; 0: edge trigger */
#define IRQF_PT		(1U << 2U)	/* 1: for passthrough dev */

extern uint64_t irq_alloc_bitmap[IRQ_ALLOC_BITMAP_SIZE];

typedef void (*irq_action_t)(uint32_t irq, void *priv_data);

/**
 * @brief Interrupt descriptor
 *
 * Any field change in below required lock protection with irqsave
 */
struct irq_desc {
	uint32_t irq;		/**< index to irq_desc_base */

	void *arch_data;	/**< arch-specific data */

	irq_action_t action;	/**< callback registered from component */
	void *priv_data;	/**< irq_action private data */
	uint32_t flags;		/**< flags for trigger mode/ptdev */

	spinlock_t lock;
};

/**
 * @defgroup phys_int_ext_apis Physical Interrupt External Interfaces
 *
 * This is a group that includes Physical Interrupt External Interfaces.
 *
 * @{
 */

/**
 * @brief Reserve an interrupt num
 *
 * Reserved interrupt num will not be available for dynamic IRQ allocations.
 * This is normally used by the hypervisor for static IRQ mappings and/or
 * arch specific, e.g. IOAPIC, interrupts during initialization.
 *
 * @param[in]	req_irq	irq_num to be reserved
 *
 * @retval >=0 on success, IRQ_INVALID on failure
 */
uint32_t reserve_irq_num(uint32_t req_irq);

/**
 * @brief Request an interrupt
 *
 * Request interrupt num if not specified, and register irq action for the
 * specified/allocated irq.
 *
 * @param[in]	req_irq	irq_num to request, if IRQ_INVALID, a free irq
 *		number will be allocated
 * @param[in]	action_fn	Function to be called when the IRQ occurs
 * @param[in]	priv_data	Private data for action function.
 * @param[in]	flags	Interrupt type flags, including:
 *			IRQF_NONE;
 *			IRQF_LEVEL - 1: level trigger; 0: edge trigger;
 *			IRQF_PT    - 1: for passthrough dev
 *
 * @retval >=0 on success
 * @retval IRQ_INVALID on failure
 */
int32_t request_irq(uint32_t req_irq, irq_action_t action_fn, void *priv_data,
			uint32_t flags);

/**
 * @brief Free an interrupt
 *
 * Free irq num and unregister the irq action.
 *
 * @param[in]	irq	irq_num to be freed
 */
void free_irq(uint32_t irq);

/**
 * @brief Set interrupt trigger mode
 *
 * Set the irq trigger mode: edge-triggered or level-triggered
 *
 * @param[in]	irq	irq_num of interrupt to be set
 * @param[in]	is_level_triggered	Trigger mode to set
 */
void set_irq_trigger_mode(uint32_t irq, bool is_level_triggered);

/**
 * @brief Process an IRQ
 *
 * To process an IRQ, an action callback will be called if registered.
 *
 * @param irq     irq_num to be processed
 */
void do_irq(const uint32_t irq);

/**
 * @brief Initialize interrupt
 *
 * To do interrupt initialization for a cpu, will be called for each physical cpu.
 *
 * @param[in]	pcpu_id The id of physical cpu to initialize
 */
void init_interrupt(uint16_t pcpu_id);


/**
 * @}
 */
/* End of phys_int_ext_apis */

#endif /* COMMON_IRQ_H */
