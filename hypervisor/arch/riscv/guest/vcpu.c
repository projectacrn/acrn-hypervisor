/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vcpu.h>
#include <irq.h>
#include <bits.h>
#include <event.h>
#include <vm.h>

int32_t arch_init_vcpu(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
	return 0;
}

void arch_deinit_vcpu(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
}

void arch_vcpu_thread(struct thread_object *obj)
{
	(void)obj;
}

void arch_reset_vcpu(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
}

void arch_context_switch_out(struct thread_object *prev)
{
	(void)prev;
}

void arch_context_switch_in(struct thread_object *next)
{
	(void)next;
}

uint64_t arch_build_stack_frame(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
	return 0;
}
