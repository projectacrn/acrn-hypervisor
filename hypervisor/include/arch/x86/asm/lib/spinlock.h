/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef X86_LIB_SPINLOCK_H
#define X86_LIB_SPINLOCK_H
#ifndef ASSEMBLER
/** The architecture dependent spinlock type. */
typedef struct _arch_spinlock {
	uint32_t head;
	uint32_t tail;
} arch_spinlock_t;

static inline void arch_spinlock_obtain(arch_spinlock_t *lock)
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

static inline void arch_spinlock_release(arch_spinlock_t *lock)
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

.macro arch_spinlock_obtain lock_arg
	movl $1, % eax
	lea \lock_arg, % rbx
	lock xaddl % eax, SYNC_SPINLOCK_HEAD_OFFSET(%rbx)
	cmpl % eax, SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
	jz 1f
2 :
	pause
	cmpl % eax, SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
	jnz 2b
1 :
.endm
#define arch_spinlock_obtain(x) arch_spinlock_obtain lock_arg = (x)

.macro arch_spinlock_release lock_arg
	lea \lock_arg, % rbx
	lock incl SYNC_SPINLOCK_TAIL_OFFSET(%rbx)
.endm
#define arch_spinlock_release(x) arch_spinlock_release lock_arg = (x)

#endif	/* ASSEMBLER */
#endif /* X86_LIB_SPINLOCK_H */
