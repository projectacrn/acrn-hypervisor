/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <schedule.h>

static uint64_t pcpu_used_bitmap;
static struct sched_object idle;

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
		ctx->curr_obj = NULL;
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

void add_to_cpu_runqueue(struct sched_object *obj, uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	spinlock_obtain(&ctx->runqueue_lock);
	if (list_empty(&obj->run_list)) {
		list_add_tail(&obj->run_list, &ctx->runqueue);
	}
	spinlock_release(&ctx->runqueue_lock);
}

void remove_from_cpu_runqueue(struct sched_object *obj, uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	spinlock_obtain(&ctx->runqueue_lock);
	list_del_init(&obj->run_list);
	spinlock_release(&ctx->runqueue_lock);
}

static struct sched_object *get_next_sched_obj(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);
	struct sched_object *obj = NULL;

	spinlock_obtain(&ctx->runqueue_lock);
	if (!list_empty(&ctx->runqueue)) {
		obj = get_first_item(&ctx->runqueue, struct sched_object, run_list);
	}
	spinlock_release(&ctx->runqueue_lock);

	return obj;
}

void make_reschedule_request(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	bitmap_set_lock(NEED_RESCHEDULE, &ctx->flags);
	if (get_cpu_id() != pcpu_id) {
		send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
	}
}

int32_t need_reschedule(uint16_t pcpu_id)
{
	struct sched_context *ctx = &per_cpu(sched_ctx, pcpu_id);

	return bitmap_test_and_clear_lock(NEED_RESCHEDULE, &ctx->flags);
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

static void switch_to_asm(struct sched_object *next, uint64_t cur_sp)
{
	asm volatile ("movq %2, %%rsp\n"
			"movq %0, %%rdi\n"
			"call 22f\n"
			"11: \n"
			"pause\n"
			"jmp 11b\n"
			"22:\n"
			"mov %1, (%%rsp)\n"
			"ret\n"
			:
			: "c"(next), "a"(next->thread), "r"(cur_sp)
			: "memory");
}

static void switch_to(struct sched_object *next)
{
	/*
	 * reset stack pointer here. Otherwise, schedule
	 * is recursive call and stack will overflow finally.
	 */
	uint64_t cur_sp = (uint64_t)&get_cpu_var(stack)[CONFIG_STACK_SIZE];

	switch_to_asm(next, cur_sp);
}

static void prepare_switch(struct sched_object *prev, struct sched_object *next)
{
	if ((prev != NULL) && (prev->prepare_switch_out != NULL)) {
		prev->prepare_switch_out(prev);
	}

	/* update current object */
	get_cpu_var(sched_ctx).curr_obj = next;

	if ((next != NULL) && (next->prepare_switch_in != NULL)) {
		next->prepare_switch_in(next);
	}
}

void schedule(void)
{
	uint16_t pcpu_id = get_cpu_id();
	struct sched_object *next = NULL;
	struct sched_object *prev = per_cpu(sched_ctx, pcpu_id).curr_obj;

	get_schedule_lock(pcpu_id);
	next = get_next_sched_obj(pcpu_id);

	if (prev == next) {
		release_schedule_lock(pcpu_id);
		return;
	}

	prepare_switch(prev, next);
	release_schedule_lock(pcpu_id);

	if (next == NULL) {
		next = &idle;
	}

	switch_to(next);

	ASSERT(false, "Shouldn't go here");
}

void switch_to_idle(run_thread_t idle_thread)
{
	uint16_t pcpu_id = get_cpu_id();

	if (pcpu_id == BOOT_CPU_ID) {
		idle.thread = idle_thread;
		idle.prepare_switch_out = NULL;
		idle.prepare_switch_in = NULL;
	}

	if (idle_thread != NULL) {
		idle_thread(&idle);
	}
}
