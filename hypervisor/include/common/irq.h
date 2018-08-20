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

enum irq_mode {
	IRQ_PULSE,
	IRQ_ASSERT,
	IRQ_DEASSERT,
};

enum irq_use_state {
	IRQ_NOT_ASSIGNED = 0,
	IRQ_ASSIGNED,
};

typedef int (*irq_action_t)(uint32_t irq, void *priv_data);

/* any field change in below required irq_lock protection with irqsave */
struct irq_desc {
	uint32_t irq;		/* index to irq_desc_base */
	enum irq_use_state used;	/* this irq have assigned to device */
	uint32_t vector;	/* assigned vector */

	irq_action_t action;	/* callback registered from component */
	void *priv_data;	/* irq_action private data */
	uint32_t flags;		/* flags for trigger mode/ptdev */

	spinlock_t lock;
};

int32_t request_irq(uint32_t req_irq, irq_action_t action_fn, void *priv_data,
			uint32_t flags);

void free_irq(uint32_t irq);

void set_irq_trigger_mode(uint32_t irq, bool is_level_trigger);
#endif /* COMMON_IRQ_H */
