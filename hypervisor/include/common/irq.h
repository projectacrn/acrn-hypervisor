/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_IRQ_H
#define COMMON_IRQ_H

#define IRQF_NONE	(0U)
#define IRQF_LEVEL	(1U << 1U)	/* 1: level trigger; 0: edge trigger */
#define IRQF_PT		(1U << 2U)	/* 1: for passthrough dev */


typedef void (*irq_action_t)(uint32_t irq, void *priv_data);

/**
 * @brief Interrupt descriptor
 *
 * Any field change in below required lock protection with irqsave
 */
struct irq_desc {
	uint32_t irq;		/**< index to irq_desc_base */
	uint32_t vector;	/**< assigned vector */

	irq_action_t action;	/**< callback registered from component */
	void *priv_data;	/**< irq_action private data */
	uint32_t flags;		/**< flags for trigger mode/ptdev */

	spinlock_t lock;
#ifdef PROFILING_ON
	uint64_t ctx_rip;
	uint64_t ctx_rflags;
	uint64_t ctx_cs;
#endif
};

/**
 * @brief Request an interrupt
 *
 * Request interrupt num if not specified, and register irq action for the
 * specified/allocated irq.
 *
 * @param[in]	req_irq	irq_num to request, if IRQ_INVALID, a free irq
 *		number will be allocated
 * @param[in]	action_fn	Function to be called when the IRQ occurs
 * @param[in]	priv_data 	Private data for action function.
 * @param[in]	flags	Interrupt type flags, including:
 *			IRQF_NONE;
 *			IRQF_LEVEL - 1: level trigger; 0: edge trigger;
 *    			IRQF_PT    - 1: for passthrough dev
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
 * @param[in]	irq	irq_num of interupt to be set
 * @param[in]	is_level_triggered	Trigger mode to set
 */
void set_irq_trigger_mode(uint32_t irq, bool is_level_triggered);
#endif /* COMMON_IRQ_H */
