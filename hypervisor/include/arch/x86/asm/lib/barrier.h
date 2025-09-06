/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef X86_LIB_BARRIER_H
#define X86_LIB_BARRIER_H
/* Synchronizes all read accesses to memory */
static inline void arch_cpu_read_memory_barrier(void)
{
	asm volatile ("lfence\n" : : : "memory");
}

/* Synchronizes all write accesses to memory */
static inline void arch_cpu_write_memory_barrier(void)
{
	asm volatile ("sfence\n" : : : "memory");
}

/* Synchronizes all read and write accesses to/from memory */
static inline void arch_cpu_memory_barrier(void)
{
	asm volatile ("mfence\n" : : : "memory");
}
#endif /* X86_LIB_BARRIER_H */
