/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <softirq.h>
#include <asm/timer.h>
#include "trap.h"

#define MAX_IRQ_HANDLER		6

/*
 * FIXME:
 * This logic need to be refined once irq multi-arch framework refine work
 * is done. Exception code 1(IPI), 5(timer) and 9(ext int) will be merged
 * into irq num namespace together, and keep a same entry in the exception
 * code table. Abstract PLIC/AIA as a irqchip that implement a get_irq API
 * to do the mapping between irq_num and PLIC source id or AIA's MSI.
 */
void sexpt_handler(void)
{
	/* TODO: add early_printk here to announce Panic */
	while(1) {};
}

static void stimer_handler(void)
{
	timer_irq_handler();
}

/* The irq handler array is not complete. So far, only timer handler is added. */
static irq_handler_t sirq_handler[] = {
	sexpt_handler,
	sexpt_handler,
	sexpt_handler,
	sexpt_handler,
	sexpt_handler,
	stimer_handler,
	sexpt_handler,
};

void sint_handler(int irq)
{
	if (irq < MAX_IRQ_HANDLER) {
		sirq_handler[irq]();
	} else {
		sirq_handler[MAX_IRQ_HANDLER]();
	}

	do_softirq();
}
