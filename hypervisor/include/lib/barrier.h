/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BARRIER_H
#define BARRIER_H

#include <types.h>
#include <asm/lib/barrier.h>

/* The mandatory functions should be implemented by arch barrier library */
static inline void arch_cpu_read_memory_barrier(void);
static inline void arch_cpu_write_memory_barrier(void);
static inline void arch_cpu_memory_barrier(void);

/* The common functions map to arch implementation */
/* Synchronizes all write accesses to memory */
static inline void cpu_write_memory_barrier(void)
{
        return arch_cpu_write_memory_barrier();
}
/* Synchronizes all read accesses from memory */
static inline void cpu_read_memory_barrier(void)
{
        return arch_cpu_read_memory_barrier();
}
/* Synchronizes all read and write accesses to/from memory */
static inline void cpu_memory_barrier(void)
{
        return arch_cpu_memory_barrier();
}

/* Prevents compilers from reordering read/write access across this barrier */
static inline void cpu_compiler_barrier(void)
{
        asm volatile ("" : : : "memory");
}
#endif /* BARRIER_H */
