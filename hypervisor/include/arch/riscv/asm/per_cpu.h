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

#include <types.h>
#include <asm/page.h>

struct per_cpu_arch {

} __aligned(PAGE_SIZE); /* per_cpu_region size aligned with PAGE_SIZE */


#endif /* RISCV_PERCPU_H */
