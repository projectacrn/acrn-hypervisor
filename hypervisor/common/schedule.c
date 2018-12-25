/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>

static uint64_t pcpu_used_bitmap;

void init_scheduler(void)
{
	struct sched_context *ctx;
	uint32_t i;

	for (i = 0U; i < phys_cpu_num; i++) {
		ctx = &per_cpu(sched_ctx, i);

		spinlock_init(&ctx->runqueue_lock);
		spinlock_init(&ctx->scheduler_lock);
		INIT_LIST_HEAD(&ctx->runqueue);
		ctx->flags = 0UL;
		ctx->curr_vcpu = NULL;
	}
}

void get_schedule_lock(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	spinlock_obtain(&ctx->scheduler_lock);
}

void release_schedule_lock(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	spinlock_release(&ctx->scheduler_lock);
}

uint16_t allocate_pcpu(void)
{
	uint16_t i;

	for (i = 0U; i < phys_cpu_num; i++) {
		if (bitmap_test_and_set_lock(i, &pcpu_used_bitmap) == 0) {
			return i;
		}
	}

	return INVALID_CPU_ID;
}

void set_pcpu_used(uint16_t pcpu_id)
{
	bitmap_set_lock(pcpu_id, &pcpu_used_bitmap);
}

void free_pcpu(uint16_t pcpu_id)
{
	bitmap_clear_lock(pcpu_id, &pcpu_used_bitmap);
}

void add_vcpu_to_runqueue(struct acrn_vcpu *vcpu)
{
	uint16_t pcpu_id = vcpu->pcpu_id;
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	spinlock_obtain(&ctx->runqueue_lock);
	if (list_empty(&vcpu->run_list)) {
		list_add_tail(&vcpu->run_list, &ctx->runqueue);
	}
	spinlock_release(&ctx->runqueue_lock);
}

void remove_vcpu_from_runqueue(struct acrn_vcpu *vcpu)
{
	uint16_t pcpu_id = vcpu->pcpu_id;
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	spinlock_obtain(&ctx->runqueue_lock);
	list_del_init(&vcpu->run_list);
	spinlock_release(&ctx->runqueue_lock);
}

static struct acrn_vcpu *select_next_vcpu(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	struct acrn_vcpu *vcpu = NULL;

	spinlock_obtain(&ctx->runqueue_lock);
	if (!list_empty(&ctx->runqueue)) {
		vcpu = get_first_item(&ctx->runqueue, struct acrn_vcpu, run_list);
	}
	spinlock_release(&ctx->runqueue_lock);

	return vcpu;
}

void make_reschedule_request(const struct acrn_vcpu *vcpu)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, vcpu->pcpu_id);

	bitmap_set_lock(NEED_RESCHEDULE, &ctx->flags);
	if (get_cpu_id() != vcpu->pcpu_id) {
		send_single_ipi(vcpu->pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

bool need_reschedule(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	return bitmap_test(NEED_RESCHEDULE, &ctx->flags);
}

static void context_switch_out(struct acrn_vcpu *vcpu)
{
	/* if it's idle thread, no action for switch out */
	if (vcpu == NULL) {
		return;
	}

	/* cancel event(int, gp, nmi and exception) injection */
	cancel_event_injection(vcpu);

	atomic_store32(&vcpu->running, 0U);
	/* do prev vcpu context switch out */
	/* For now, we don't need to invalid ept.
	 * But if we have more than one vcpu on one pcpu,
	 * we need add ept invalid operation here.
	 */
}

static void context_switch_in(struct acrn_vcpu *vcpu)
{
	/* update current_vcpu */
	get_cpu_var(sched_ctx).curr_vcpu = vcpu;

	/* if it's idle thread, no action for switch out */
	if (vcpu == NULL) {
		return;
	}

	atomic_store32(&vcpu->running, 1U);
	/* FIXME:
	 * Now, we don't need to load new vcpu VMCS because
	 * we only do switch between vcpu loop and idle loop.
	 * If we have more than one vcpu on on pcpu, need to
	 * add VMCS load operation here.
	 */
}

void make_pcpu_offline(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	bitmap_set_lock(NEED_OFFLINE, &ctx->flags);
	if (get_cpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

int32_t need_offline(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	return bitmap_test_and_clear_lock(NEED_OFFLINE, &ctx->flags);
}

void default_idle(void)
{
	uint16_t pcpu_id = get_cpu_id();

	while (1) {
		if (need_reschedule(pcpu_id)) {
			schedule();
		} else if (need_offline(pcpu_id) != 0) {
			cpu_dead();
		} else {
			CPU_IRQ_ENABLE();
			handle_complete_ioreq(pcpu_id);
			cpu_do_idle();
			CPU_IRQ_DISABLE();
		}
	}
}

static void switch_to(struct acrn_vcpu *curr)
{
	/*
	 * reset stack pointer here. Otherwise, schedule
	 * is recursive call and stack will overflow finally.
	 */
	uint64_t cur_sp = (uint64_t)&get_cpu_var(stack)[CONFIG_STACK_SIZE];

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
	uint16_t pcpu_id = get_cpu_id();
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	struct acrn_vcpu *next = NULL;
	struct acrn_vcpu *prev = ctx->curr_vcpu;

	get_schedule_lock(pcpu_id);
	next = select_next_vcpu(pcpu_id);
	bitmap_clear_lock(NEED_RESCHEDULE, &ctx->flags);

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
