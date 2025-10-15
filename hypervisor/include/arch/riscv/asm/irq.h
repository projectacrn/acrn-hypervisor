/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_IRQ_H
#define RISCV_IRQ_H

#include <cpu.h>

#define IPI_NOTIFY_CPU		0U

/* TODO: to be implemented in later patches when integration with the common IRQ framework is done.*/
#define NR_IRQS			0U

struct intr_excp_ctx {
	struct cpu_regs regs;
};

void init_interrupt(uint16_t pcpu_id);

#endif /* RISCV_IRQ_H */
