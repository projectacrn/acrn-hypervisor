/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef __RISCV_TIMER_H__
#define __RISCV_TIMER_H__

#include <timer.h>

/* FIXME:
 * This header file might be removed once irq multi-arch framework refine work is done.
 * Afther that, the timer_irq_handler will be registered as irq handler when executing
 * request_irq().
 */
void timer_irq_handler(void);

#endif /* __RISCV_TIMER_H__ */
