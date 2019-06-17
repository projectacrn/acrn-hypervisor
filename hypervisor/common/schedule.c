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

bool is_idle_thread(const struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;
	return (obj == &per_cpu(idle, pcpu_id));
}

static inline bool is_blocked(const struct thread_object *obj)
{
	return obj->status == THREAD_STS_BLOCKED;
}

static inline bool is_runnable(const struct thread_object *obj)
{
	return obj->status == THREAD_STS_RUNNABLE;
}

static inline bool is_running(const struct thread_object *obj)
{
	return obj->status == THREAD_STS_RUNNING;
}

static inline void set_thread_status(struct thread_object *obj, enum thread_object_state status)
{
	obj->status = status;
}

/**
 * @pre obj != NULL
 */
uint16_t sched_get_pcpuid(const struct thread_object *obj)
{
	return obj->pcpu_id;
}

void init_sched(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	spinlock_init(&ctl->scheduler_lock);
	ctl->flags = 0UL;
	ctl->curr_obj = NULL;
	ctl->pcpu_id = pcpu_id;
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

void insert_thread_obj(struct thread_object *obj, uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	ctl->thread_obj = obj;
}

void remove_thread_obj(__unused struct thread_object *obj, uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	ctl->thread_obj = NULL;
}

static struct thread_object *get_next_sched_obj(const struct sched_control *ctl)
{
	return ctl->thread_obj == NULL ? &get_cpu_var(idle) : ctl->thread_obj;
}

struct thread_object *sched_get_current(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	return ctl->curr_obj;
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

	/* Don't change prev object's status if it's not running */
	if (is_running(prev)) {
		set_thread_status(prev, THREAD_STS_RUNNABLE);
	}
	set_thread_status(next, THREAD_STS_RUNNING);

	if (prev == next) {
		release_schedule_lock(pcpu_id);
	} else {
		do_switch(prev, next);
		release_schedule_lock(pcpu_id);

		arch_switch_to(&prev->host_sp, &next->host_sp);
	}
}

void sleep_thread(struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;

	get_schedule_lock(pcpu_id);
	remove_thread_obj(obj, pcpu_id);
	if (is_running(obj)) {
		if (obj->notify_mode == SCHED_NOTIFY_INIT) {
			make_reschedule_request(pcpu_id, DEL_MODE_INIT);
		} else {
			make_reschedule_request(pcpu_id, DEL_MODE_IPI);
		}
	}
	set_thread_status(obj, THREAD_STS_BLOCKED);
	release_schedule_lock(pcpu_id);
}

void wake_thread(struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;

	get_schedule_lock(pcpu_id);
	if (is_blocked(obj)) {
		insert_thread_obj(obj, pcpu_id);
		set_thread_status(obj, THREAD_STS_RUNNABLE);
		make_reschedule_request(pcpu_id, DEL_MODE_IPI);
	}
	release_schedule_lock(pcpu_id);
}

void run_thread(struct thread_object *obj)
{
	get_schedule_lock(obj->pcpu_id);
	get_cpu_var(sched_ctl).curr_obj = obj;
	set_thread_status(obj, THREAD_STS_RUNNING);
	release_schedule_lock(obj->pcpu_id);

	if (obj->thread_entry != NULL) {
		obj->thread_entry(obj);
	}
}
