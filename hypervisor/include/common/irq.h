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

enum irq_state {
	IRQ_NOT_ASSIGNED = 0,
	IRQ_ASSIGNED_SHARED,
	IRQ_ASSIGNED_NOSHARE,
};

enum irq_desc_state {
	IRQ_DESC_PENDING,
	IRQ_DESC_IN_PROCESS,
};

typedef int (*dev_handler_t)(int irq, void *dev_data);
struct irq_request_info {
	/* vector set to 0xE0 ~ 0xFF for pri_register_handler
	 * and set to VECTOR_INVALID for normal_register_handler
	 */
	uint32_t vector;
	dev_handler_t func;
	void *dev_data;
	bool share;
	bool lowpri;
	char *name;
};

/* any field change in below required irq_lock protection with irqsave */
struct irq_desc {
	uint32_t irq;		/* index to irq_desc_base */
	enum irq_state used;	/* this irq have assigned to device */
	enum irq_desc_state state; /* irq_desc status */
	uint32_t vector;	/* assigned vector */
	void *handler_data;	/* irq_handler private data */
	int (*irq_handler)(struct irq_desc *irq_desc, void *handler_data);
	struct dev_handler_node *dev_list;
	spinlock_t irq_lock;
	uint64_t *irq_cnt; /* this irq cnt happened on CPUs */
	uint64_t irq_lost_cnt;
};

struct dev_handler_node {
	char name[32];
	void *dev_data;
	dev_handler_t dev_handler;
	struct dev_handler_node *next;
	struct irq_desc *desc;
};

uint32_t irq_mark_used(uint32_t irq);

uint32_t irq_desc_alloc_vector(uint32_t irq, bool lowpri);
void irq_desc_try_free_vector(uint32_t irq);

uint32_t irq_to_vector(uint32_t irq);
uint32_t dev_to_irq(struct dev_handler_node *node);
uint32_t dev_to_vector(struct dev_handler_node *node);

struct dev_handler_node*
pri_register_handler(uint32_t irq,
		uint32_t vector,
		dev_handler_t func,
		void *dev_data,
		const char *name);

struct dev_handler_node*
normal_register_handler(uint32_t irq,
		dev_handler_t func,
		void *dev_data,
		bool share,
		bool lowpri,
		const char *name);
void unregister_handler_common(struct dev_handler_node *node);

typedef int (*irq_handler_t)(struct irq_desc *desc, void *handler_data);
void update_irq_handler(uint32_t irq, irq_handler_t func);

#endif /* COMMON_IRQ_H */
