/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#ifndef ASSEMBLER

#include <types.h>
#include <rtl.h>

/** The architecture dependent spinlock type. */
typedef struct _spinlock {
	uint32_t head;
	uint32_t tail;

} spinlock_t;

/* Function prototypes */
static inline void spinlock_init(spinlock_t *lock)
{
	(void)memset(lock, 0U, sizeof(spinlock_t));
}

static inline void spinlock_obtain(spinlock_t *lock)
{

	/* The lock function atomically increments and exchanges the head
	 * counter of the queue. If the old head of the queue is equal to the
	 * tail, we have locked the spinlock. Otherwise we have to wait.
	 */

	asm volatile ("   movl $0x1,%%eax\n"
		      "   lock xaddl %%eax,%[head]\n"
		      "   cmpl %%eax,%[tail]\n"
		      "   jz 1f\n"
		      "2: pause\n"
		      "   cmpl %%eax,%[tail]\n"
		      "   jnz 2b\n"
		      "1:\n"
		      :
		      :
		      [head] "m"(lock->head),
		      [tail] "m"(lock->tail)
		      : "cc", "memory", "eax");
}

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

#define spinlock_irqsave_obtain(lock, p_rflags)		\
	do {						\
		CPU_INT_ALL_DISABLE(p_rflags);		\
		spinlock_obtain(lock);			\
	} while (0)

#define spinlock_irqrestore_release(lock, rflags)	\
	do {						\
		spinlock_release(lock);			\
		CPU_INT_ALL_RESTORE(rflags);		\
	} while (0)
#endif /* SPINLOCK_H */
