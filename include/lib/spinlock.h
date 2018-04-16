/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <hypervisor.h>

#ifndef ASSEMBLER

#include <types.h>

/** The architecture dependent spinlock type. */
typedef struct _spinlock {
	uint32_t head;
	uint32_t tail;

} spinlock_t;

/* Function prototypes */
int spinlock_init(spinlock_t *lock);
int spinlock_obtain(spinlock_t *lock);

static inline int spinlock_release(spinlock_t *lock)
{
	/* Increment tail of queue */
	asm volatile ("   lock incl %[tail]\n"
				:
				: [tail] "m" (lock->tail)
				: "cc", "memory");

	return 0;
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
		spinlock_obtain((l));			\
	} while (0)

#define spinlock_irqrestore_release(l)			\
	do {						\
		spinlock_release((l));			\
		CPU_INT_ALL_RESTORE();			\
	} while (0)

#endif /* SPINLOCK_H */
