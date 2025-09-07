/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_LIB_BARRIER_H
#define RISCV_LIB_BARRIER_H
/* Synchronizes all read accesses to/from memory */
static inline void arch_cpu_read_memory_barrier(void)
{
	asm volatile ("fence r,r" : : : "memory");
}

static inline void arch_cpu_write_memory_barrier(void)
{
	asm volatile ("fence w,w" : : : "memory");
}

/* Synchronizes all read and write accesses to/from memory */
static inline void arch_cpu_memory_barrier(void)
{
	asm volatile ("fence rw,rw" : : : "memory");
}
#endif /* RISCV_LIB_BARRIER_H */
