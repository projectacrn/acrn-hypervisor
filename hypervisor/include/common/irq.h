/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_IRQ_H
#define COMMON_IRQ_H

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

	int (*irq_handler)(struct irq_desc *irq_desc, void *handler_data);
				/* callback for irq flow handling */
	irq_action_t action;	/* callback registered from component */
	void *priv_data;	/* irq_action private data */

	spinlock_t lock;
};

uint32_t irq_desc_alloc_vector(uint32_t irq);
void irq_desc_try_free_vector(uint32_t irq);

uint32_t irq_to_vector(uint32_t irq);

int32_t request_irq(uint32_t irq,
		    irq_action_t action_fn,
		    void *priv_data);

void free_irq(uint32_t irq);

typedef int (*irq_handler_t)(struct irq_desc *desc, void *handler_data);
void update_irq_handler(uint32_t irq, irq_handler_t func);
#endif /* COMMON_IRQ_H */
