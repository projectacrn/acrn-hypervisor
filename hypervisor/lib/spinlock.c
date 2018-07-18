/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hv_lib.h>

inline void spinlock_init(spinlock_t *lock)
{
	(void)memset(lock, 0U, sizeof(spinlock_t));
}
void spinlock_obtain(spinlock_t *lock)
{

	/* The lock function atomically increments and exchanges the head
	 * counter of the queue. If the old head of the queue is equal to the
	 * tail, we have locked the spinlock. Otherwise we have to wait.
	 */

	asm volatile ("   lock xaddl %%eax,%[head]\n"
		      "   cmpl %%eax,%[tail]\n"
		      "   jz 1f\n"
		      "2: pause\n"
		      "   cmpl %%eax,%[tail]\n"
		      "   jnz 2b\n"
		      "1:\n"
		      :
		      : "a" (1),
		      [head] "m"(lock->head),
		      [tail] "m"(lock->tail)
		      : "cc", "memory");
}
