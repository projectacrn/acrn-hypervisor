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

void obtain_schedule_lock(uint16_t pcpu_id, uint64_t *rflag)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	spinlock_irqsave_obtain(&ctl->scheduler_lock, rflag);
}

void release_schedule_lock(uint16_t pcpu_id, uint64_t rflag)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	spinlock_irqrestore_release(&ctl->scheduler_lock, rflag);
}

static struct acrn_scheduler *get_scheduler(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	return ctl->scheduler;
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
#ifdef CONFIG_SCHED_NOOP
	ctl->scheduler = &sched_noop;
#endif
#ifdef CONFIG_SCHED_IORR
	ctl->scheduler = &sched_iorr;
#endif
#ifdef CONFIG_SCHED_BVT
	ctl->scheduler = &sched_bvt;
#endif
	if (ctl->scheduler->init != NULL) {
		ctl->scheduler->init(ctl);
	}
}

void deinit_sched(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	if (ctl->scheduler->deinit != NULL) {
		ctl->scheduler->deinit(ctl);
	}
}

void init_thread_data(struct thread_object *obj)
{
	struct acrn_scheduler *scheduler = get_scheduler(obj->pcpu_id);
	uint64_t rflag;

	obtain_schedule_lock(obj->pcpu_id, &rflag);
	if (scheduler->init_data != NULL) {
		scheduler->init_data(obj);
	}
	/* initial as BLOCKED status, so we can wake it up to run */
	set_thread_status(obj, THREAD_STS_BLOCKED);
	release_schedule_lock(obj->pcpu_id, rflag);
}

void deinit_thread_data(struct thread_object *obj)
{
	struct acrn_scheduler *scheduler = get_scheduler(obj->pcpu_id);

	if (scheduler->deinit_data != NULL) {
		scheduler->deinit_data(obj);
	}
}

struct thread_object *sched_get_current(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	return ctl->curr_obj;
}

/**
 * @pre delmode == DEL_MODE_IPI || delmode == DEL_MODE_NMI
 */
void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	bitmap_set_lock(NEED_RESCHEDULE, &ctl->flags);
	if (get_pcpu_id() != pcpu_id) {
		switch (delmode) {
		case DEL_MODE_IPI:
			send_single_ipi(pcpu_id, NOTIFY_VCPU_VECTOR);
			break;
		case DEL_MODE_NMI:
			send_single_nmi(pcpu_id);
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

void schedule(void)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);
	struct thread_object *next = &per_cpu(idle, pcpu_id);
	struct thread_object *prev = ctl->curr_obj;
	uint64_t rflag;

	obtain_schedule_lock(pcpu_id, &rflag);
	if (ctl->scheduler->pick_next != NULL) {
		next = ctl->scheduler->pick_next(ctl);
	}
	bitmap_clear_lock(NEED_RESCHEDULE, &ctl->flags);

	/* Don't change prev object's status if it's not running */
	if (is_running(prev)) {
		set_thread_status(prev, THREAD_STS_RUNNABLE);
	}
	set_thread_status(next, THREAD_STS_RUNNING);
	ctl->curr_obj = next;
	release_schedule_lock(pcpu_id, rflag);

	/* If we picked different sched object, switch context */
	if (prev != next) {
		if ((prev != NULL) && (prev->switch_out != NULL)) {
			prev->switch_out(prev);
		}

		if ((next != NULL) && (next->switch_in != NULL)) {
			next->switch_in(next);
		}

		arch_switch_to(&prev->host_sp, &next->host_sp);
	}
}

void sleep_thread(struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;
	struct acrn_scheduler *scheduler = get_scheduler(pcpu_id);
	uint64_t rflag;

	obtain_schedule_lock(pcpu_id, &rflag);
	if (scheduler->sleep != NULL) {
		scheduler->sleep(obj);
	}
	if (is_running(obj)) {
		if (obj->notify_mode == SCHED_NOTIFY_NMI) {
			make_reschedule_request(pcpu_id, DEL_MODE_NMI);
		} else {
			make_reschedule_request(pcpu_id, DEL_MODE_IPI);
		}
	}
	set_thread_status(obj, THREAD_STS_BLOCKED);
	release_schedule_lock(pcpu_id, rflag);
}

void wake_thread(struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;
	struct acrn_scheduler *scheduler;
	uint64_t rflag;

	obtain_schedule_lock(pcpu_id, &rflag);
	if (is_blocked(obj)) {
		scheduler = get_scheduler(pcpu_id);
		if (scheduler->wake != NULL) {
			scheduler->wake(obj);
		}
		set_thread_status(obj, THREAD_STS_RUNNABLE);
		make_reschedule_request(pcpu_id, DEL_MODE_IPI);
	}
	release_schedule_lock(pcpu_id, rflag);
}

void kick_thread(const struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;
	uint64_t rflag;

	obtain_schedule_lock(pcpu_id, &rflag);
	if (is_running(obj)) {
		if (get_pcpu_id() != pcpu_id) {
			if (obj->notify_mode == SCHED_NOTIFY_IPI) {
				send_single_ipi(pcpu_id, NOTIFY_VCPU_VECTOR);
			} else {
				/* For lapic-pt vCPUs */
				send_single_nmi(pcpu_id);
			}
		}
	} else if (is_runnable(obj)) {
		if (obj->notify_mode == SCHED_NOTIFY_IPI) {
			make_reschedule_request(pcpu_id, DEL_MODE_IPI);
		} else {
			/* For lapic-pt vCPUs */
			make_reschedule_request(pcpu_id, DEL_MODE_NMI);
		}
	} else {
		/* do nothing */
	}
	release_schedule_lock(pcpu_id, rflag);
}

void yield_current(void)
{
	make_reschedule_request(get_pcpu_id(), DEL_MODE_IPI);
}

void run_thread(struct thread_object *obj)
{
	uint64_t rflag;

	init_thread_data(obj);
	obtain_schedule_lock(obj->pcpu_id, &rflag);
	get_cpu_var(sched_ctl).curr_obj = obj;
	set_thread_status(obj, THREAD_STS_RUNNING);
	release_schedule_lock(obj->pcpu_id, rflag);

	if (obj->thread_entry != NULL) {
		obj->thread_entry(obj);
	}
}
