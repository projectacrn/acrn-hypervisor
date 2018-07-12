/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#ifndef ASSEMBLER

#include <types.h>

/** The architecture dependent spinlock type. */
typedef struct _spinlock {
	uint32_t head;
	uint32_t tail;

} spinlock_t;

/* Function prototypes */
void spinlock_init(spinlock_t *lock);
void spinlock_obtain(spinlock_t *lock);

static inline void spinlock_release(spinlock_t *lock)
{
	/* Increment tail of queue */
	asm volatile ("   lock incl %[tail]\n"
				:
				: [tail] "m" (lock->tail)
				: "cc", "memory");
}

#else /* ASSEMBLER */

/** The offset of the head element. */
#define SYNC_SPINLOCK_HEAD_OFFSET       0

/** The offset of the tail element. */
#define SYNC_SPINLOCK_TAIL_OFFSET       4

.macro spinlock_obtain lock
	movl $1, % eax
	lea \lock, % rbx
	lock xaddl % eax, SYNC_SPINLOCK_HEAD_OFFSET(%rbx)
	cmpl % eax, SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
	jz 1f
2 :
	pause
	cmpl % eax, SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
	jnz 2b
1 :
.endm

#define spinlock_obtain(x) spinlock_obtain lock = (x)

.macro spinlock_release lock
	lea \lock, % rbx
	lock incl SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
.endm

#define spinlock_release(x) spinlock_release lock = (x)

#endif	/* ASSEMBLER */

#define spinlock_rflags unsigned long cpu_int_value

#define spinlock_irqsave_obtain(l)			\
	do {						\
		CPU_INT_ALL_DISABLE();			\
		spinlock_obtain(l);			\
	} while (0)

#define spinlock_irqrestore_release(l)			\
	do {						\
		spinlock_release(l);			\
		CPU_INT_ALL_RESTORE();			\
	} while (0)

#endif /* SPINLOCK_H */
