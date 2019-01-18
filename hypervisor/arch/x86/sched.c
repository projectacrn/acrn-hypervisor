/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <spinlock.h>
#include <list.h>
#include <schedule.h>

void arch_switch_to(struct sched_object *prev, struct sched_object *next)
{
	asm volatile ("pushf\n"
			"pushq %%rbx\n"
			"pushq %%rbp\n"
			"pushq %%r12\n"
			"pushq %%r13\n"
			"pushq %%r14\n"
			"pushq %%r15\n"
			"pushq %%rdi\n"
			"movq %%rsp, %0\n"
			"movq %1, %%rsp\n"
			"popq %%rdi\n"
			"popq %%r15\n"
			"popq %%r14\n"
			"popq %%r13\n"
			"popq %%r12\n"
			"popq %%rbp\n"
			"popq %%rbx\n"
			"popf\n"
			"retq\n"
			: "=m"(prev->host_sp)
			: "r"(next->host_sp)
			: "memory");
}
