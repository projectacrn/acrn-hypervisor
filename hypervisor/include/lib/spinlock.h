/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#ifndef ASSEMBLER
#include <types.h>
#include <rtl.h>
#include <asm/cpu.h>
#include <asm/lib/spinlock.h>

/* The common spinlock type */
typedef arch_spinlock_t spinlock_t;

/* The mandatory functions should be implemented by arch spinlock library */
static inline void arch_spinlock_obtain(arch_spinlock_t *lock);
static inline void arch_spinlock_release(arch_spinlock_t *lock);

/* Function prototypes */
static inline void spinlock_init(spinlock_t *lock)
{
	(void)memset(lock, 0U, sizeof(spinlock_t));
}

static inline void spinlock_irqsave_obtain(spinlock_t *lock, uint64_t * flags)
{
	CPU_INT_ALL_DISABLE(flags);
	arch_spinlock_obtain(lock);
}

static inline void spinlock_irqrestore_release(spinlock_t *lock, uint64_t flags)
{
	arch_spinlock_release(lock);
	CPU_INT_ALL_RESTORE(flags);
}

static inline void spinlock_obtain(spinlock_t *lock)
{
    return arch_spinlock_obtain(lock);
}

static inline void spinlock_release(spinlock_t *lock)
{
    return arch_spinlock_release(lock);
}

#else /* ASSEMBLER */
#include <asm/lib/spinlock.h>
#define spinlock_obtain arch_spinlock_obtain
#define spinlock_release arch_spinlock_release
#endif /* ASSEMBLER */

#endif /* SPINLOCK_H */
