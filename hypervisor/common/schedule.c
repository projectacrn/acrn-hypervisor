/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>

static unsigned long pcpu_used_bitmap;

void init_scheduler(void)
{
	int i;

	for (i = 0; i < phy_cpu_num; i++) {
		spinlock_init(&per_cpu(sched_ctx, i).runqueue_lock);
		spinlock_init(&per_cpu(sched_ctx, i).scheduler_lock);
		INIT_LIST_HEAD(&per_cpu(sched_ctx, i).runqueue);
		per_cpu(sched_ctx, i).flags= 0;
		per_cpu(sched_ctx, i).curr_vcpu = NULL;
	}
}

void get_schedule_lock(int pcpu_id)
{
	spinlock_obtain(&per_cpu(sched_ctx, pcpu_id).scheduler_lock);
}

void release_schedule_lock(int pcpu_id)
{
	spinlock_release(&per_cpu(sched_ctx, pcpu_id).scheduler_lock);
}

int allocate_pcpu(void)
{
	int i;

	for (i = 0; i < phy_cpu_num; i++) {
		if (bitmap_test_and_set(i, &pcpu_used_bitmap) == 0)
			return i;
	}

	return -1;
}

void set_pcpu_used(int pcpu_id)
{
	bitmap_set(pcpu_id, &pcpu_used_bitmap);
}

void free_pcpu(int pcpu_id)
{
	bitmap_clear(pcpu_id, &pcpu_used_bitmap);
}

void add_vcpu_to_runqueue(struct vcpu *vcpu)
{
	int pcpu_id = vcpu->pcpu_id;

	spinlock_obtain(&per_cpu(sched_ctx, pcpu_id).runqueue_lock);
	if (list_empty(&vcpu->run_list))
		list_add_tail(&vcpu->run_list,
			&per_cpu(sched_ctx, pcpu_id).runqueue);
	spinlock_release(&per_cpu(sched_ctx, pcpu_id).runqueue_lock);
}

void remove_vcpu_from_runqueue(struct vcpu *vcpu)
{
	int pcpu_id = vcpu->pcpu_id;

	spinlock_obtain(&per_cpu(sched_ctx, pcpu_id).runqueue_lock);
	list_del_init(&vcpu->run_list);
	spinlock_release(&per_cpu(sched_ctx, pcpu_id).runqueue_lock);
}

static struct vcpu *select_next_vcpu(int pcpu_id)
{
	struct vcpu *vcpu = NULL;

	spinlock_obtain(&per_cpu(sched_ctx, pcpu_id).runqueue_lock);
	if (!list_empty(&per_cpu(sched_ctx, pcpu_id).runqueue)) {
		vcpu = get_first_item(&per_cpu(sched_ctx, pcpu_id).runqueue,
				struct vcpu, run_list);
	}
	spinlock_release(&per_cpu(sched_ctx, pcpu_id).runqueue_lock);

	return vcpu;
}

void make_reschedule_request(struct vcpu *vcpu)
{
	bitmap_set(NEED_RESCHEDULE,
		&per_cpu(sched_ctx, vcpu->pcpu_id).flags);
	send_single_ipi(vcpu->pcpu_id, VECTOR_NOTIFY_VCPU);
}

int need_reschedule(int pcpu_id)
{
	return bitmap_test_and_clear(NEED_RESCHEDULE,
		&per_cpu(sched_ctx, pcpu_id).flags);
}

static void context_switch_out(struct vcpu *vcpu)
{
	/* if it's idle thread, no action for switch out */
	if (vcpu == NULL)
		return;

	/* cancel event(int, gp, nmi and exception) injection */
	cancel_event_injection(vcpu);

	atomic_store(&vcpu->running, 0);
	/* do prev vcpu context switch out */
	/* For now, we don't need to invalid ept.
	 * But if we have more than one vcpu on one pcpu,
	 * we need add ept invalid operation here.
	 */
}

static void context_switch_in(struct vcpu *vcpu)
{
	/* update current_vcpu */
	get_cpu_var(sched_ctx).curr_vcpu = vcpu;

	/* if it's idle thread, no action for switch out */
	if (vcpu == NULL)
		return;

	atomic_store(&vcpu->running, 1);
	/* FIXME:
	 * Now, we don't need to load new vcpu VMCS because
	 * we only do switch between vcpu loop and idle loop.
	 * If we have more than one vcpu on on pcpu, need to
	 * add VMCS load operation here.
	 */
}

void make_pcpu_offline(int pcpu_id)
{
	bitmap_set(NEED_OFFLINE,
		&per_cpu(sched_ctx, pcpu_id).flags);
	send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
}

int need_offline(int pcpu_id)
{
	return bitmap_test_and_clear(NEED_OFFLINE,
		&per_cpu(sched_ctx, pcpu_id).flags);
}

void default_idle(void)
{
	int pcpu_id = get_cpu_id();

	while (1) {
		if (need_reschedule(pcpu_id))
			schedule();
		else if (need_offline(pcpu_id))
			cpu_dead(pcpu_id);
		else
			__asm __volatile("pause" ::: "memory");
	}
}

static void switch_to(struct vcpu *curr)
{
	/*
	 * reset stack pointer here. Otherwise, schedule
	 * is recursive call and stack will overflow finally.
	 */
	uint64_t cur_sp = (uint64_t)&get_cpu_var(stack)[STACK_SIZE];

	if (curr == NULL) {
		asm volatile ("movq %1, %%rsp\n"
				"movq $0, %%rdi\n"
				"call 22f\n"
				"11: \n"
				"pause\n"
				"jmp 11b\n"
				"22:\n"
				"mov %0, (%%rsp)\n"
				"ret\n"
				:
				: "a"(default_idle), "r"(cur_sp)
				: "memory");
	} else {
		asm volatile ("movq %2, %%rsp\n"
				"movq %0, %%rdi\n"
				"call 44f\n"
				"33: \n"
				"pause\n"
				"jmp 33b\n"
				"44:\n"
				"mov %1, (%%rsp)\n"
				"ret\n"
				:
				: "c"(curr), "a"(vcpu_thread), "r"(cur_sp)
				: "memory");
	}
}

void schedule(void)
{
	int pcpu_id = get_cpu_id();
	struct vcpu *next = NULL;
	struct vcpu *prev = per_cpu(sched_ctx, pcpu_id).curr_vcpu;

	get_schedule_lock(pcpu_id);
	next = select_next_vcpu(pcpu_id);

	if (prev == next) {
		release_schedule_lock(pcpu_id);
		return;
	}

	context_switch_out(prev);
	context_switch_in(next);
	release_schedule_lock(pcpu_id);

	switch_to(next);

	ASSERT(false, "Shouldn't go here");
}
