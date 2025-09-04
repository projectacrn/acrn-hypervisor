/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_PERCPU_H
#define RISCV_PERCPU_H

#include <common/smp.h>
#include <types.h>

struct per_cpu_region {
	struct smp_call_info_data smp_call_info;
} __aligned(PAGE_SIZE); /* per_cpu_region size aligned with PAGE_SIZE */

extern struct per_cpu_region per_cpu_data[MAX_PCPU_NUM];
/*
 * get percpu data for pcpu_id.
 */
#define per_cpu(name, pcpu_id)	\
	(per_cpu_data[(pcpu_id)].name)

#endif /* RISCV_PERCPU_H */
