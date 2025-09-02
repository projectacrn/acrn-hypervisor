/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <types.h>
#include <asm/irq.h>
#include <asm/sbi.h>

void arch_init_smp_call(void)
{
	/* No special handling is required at present; this can be extended in the future if needed. */
}

void arch_smp_call_kick_pcpu(uint16_t pcpu_id)
{
	arch_send_single_ipi(pcpu_id, IPI_NOTIFY_CPU);
}
