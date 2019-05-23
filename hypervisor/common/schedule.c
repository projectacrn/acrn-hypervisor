/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rtl.h>
#include <list.h>
#include <bits.h>
#include <cpu.h>
#include <per_cpu.h>
#include <lapic.h>
#include <schedule.h>
#include <sprintf.h>

void init_scheduler(void)
{
	struct sched_control *ctl;
	uint32_t i;
	uint16_t pcpu_nums = get_pcpu_nums();

	for (i = 0U; i < pcpu_nums; i++) {
		ctl = &per_cpu(sched_ctl, i);

		spinlock_init(&ctl->scheduler_lock);
		INIT_LIST_HEAD(&ctl->runqueue);
		ctl->flags = 0UL;
		ctl->curr_obj = NULL;
	}
}

void get_schedule_lock(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	spinlock_obtain(&ctl->scheduler_lock);
}

void release_schedule_lock(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	spinlock_release(&ctl->scheduler_lock);
}

void add_to_cpu_runqueue(struct thread_object *obj, uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	if (list_empty(&obj->run_list)) {
		list_add_tail(&obj->run_list, &ctl->runqueue);
	}
}

void remove_from_cpu_runqueue(struct thread_object *obj)
{
	list_del_init(&obj->run_list);
}

static struct thread_object *get_next_sched_obj(struct sched_control *ctl)
{
	struct thread_object *obj = NULL;

	if (!list_empty(&ctl->runqueue)) {
		obj = get_first_item(&ctl->runqueue, struct thread_object, run_list);
	} else {
		obj = &get_cpu_var(idle);
	}

	return obj;
}

/**
 * @pre delmode == DEL_MODE_IPI || delmode == DEL_MODE_INIT
 */
void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	bitmap_set_lock(NEED_RESCHEDULE, &ctl->flags);
	if (get_pcpu_id() != pcpu_id) {
		switch (delmode) {
		case DEL_MODE_IPI:
			send_single_ipi(pcpu_id, VECTOR_NOTIFY_VCPU);
			break;
		case DEL_MODE_INIT:
			send_single_init(pcpu_id);
			break;
		default:
			ASSERT(false, "Unknown delivery mode %u for pCPU%u", delmode, pcpu_id);
			break;
		}
	}
}

bool need_reschedule(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	return bitmap_test(NEED_RESCHEDULE, &ctl->flags);
}

static void do_switch(struct thread_object *prev, struct thread_object *next)
{
	if ((prev != NULL) && (prev->switch_out != NULL)) {
		prev->switch_out(prev);
	}

	/* update current object */
	get_cpu_var(sched_ctl).curr_obj = next;

	if ((next != NULL) && (next->switch_in != NULL)) {
		next->switch_in(next);
	}
}

void schedule(void)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	struct thread_object *next = NULL;
	struct thread_object *prev = ctl->curr_obj;

	get_schedule_lock(pcpu_id);
	next = get_next_sched_obj(ctl);
	bitmap_clear_lock(NEED_RESCHEDULE, &ctl->flags);

	if (prev == next) {
		release_schedule_lock(pcpu_id);
	} else {
		do_switch(prev, next);
		release_schedule_lock(pcpu_id);

		arch_switch_to(&prev->host_sp, &next->host_sp);
	}
}

void run_sched_thread(struct thread_object *obj)
{
	if (obj->thread_entry != NULL) {
		obj->thread_entry(obj);
	}

	ASSERT(false, "Shouldn't go here, invalid thread entry!");
}

void switch_to_idle(thread_entry_t idle_thread)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct thread_object *idle = &per_cpu(idle, pcpu_id);
	char idle_name[16];

	snprintf(idle_name, 16U, "idle%hu", pcpu_id);
	(void)strncpy_s(idle->name, 16U, idle_name, 16U);
	idle->thread_entry = idle_thread;
	idle->switch_out = NULL;
	idle->switch_in = NULL;
	get_cpu_var(sched_ctl).curr_obj = idle;

	run_sched_thread(idle);
}
