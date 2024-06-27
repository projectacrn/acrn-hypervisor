/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rtl.h>
#include <list.h>
#include <asm/lib/bits.h>
#include <asm/cpu.h>
#include <asm/per_cpu.h>
#include <asm/lapic.h>
#include <schedule.h>
#include <sprintf.h>
#include <asm/irq.h>

bool is_idle_thread(const struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;
	return (obj == &per_cpu(idle, pcpu_id));
}

static inline bool is_blocked(const struct thread_object *obj)
{
	return obj->status == THREAD_STS_BLOCKED;
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

	per_cpu(mode_to_idle, pcpu_id) = IDLE_MODE_HLT;
	per_cpu(mode_to_kick_pcpu, pcpu_id) = DEL_MODE_IPI;

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
#ifdef CONFIG_SCHED_PRIO
	ctl->scheduler = &sched_prio;
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

void suspend_sched(void)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, BSP_CPU_ID);

	if (ctl->scheduler->suspend != NULL) {
		ctl->scheduler->suspend(ctl);
	}
}

void resume_sched(void)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, BSP_CPU_ID);

	if (ctl->scheduler->resume != NULL) {
		ctl->scheduler->resume(ctl);
	}
}

void init_thread_data(struct thread_object *obj, struct sched_params *params)
{
	struct acrn_scheduler *scheduler = get_scheduler(obj->pcpu_id);
	uint64_t rflag;

	obtain_schedule_lock(obj->pcpu_id, &rflag);
	if (scheduler->init_data != NULL) {
		scheduler->init_data(obj, params);
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
 * @pre delmode == DEL_MODE_IPI || delmode == DEL_MODE_INIT
 */
void make_reschedule_request(uint16_t pcpu_id)
{
	struct sched_control *ctl = &per_cpu(sched_ctl, pcpu_id);

	bitmap_set_lock(NEED_RESCHEDULE, &ctl->flags);
	if (get_pcpu_id() != pcpu_id) {
		kick_pcpu(pcpu_id);
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

	/* If we picked different sched object, switch context */
	if (prev != next) {
		if (prev != NULL) {
			if (prev->switch_out != NULL) {
				prev->switch_out(prev);
			}
			set_thread_status(prev, prev->be_blocking ? THREAD_STS_BLOCKED : THREAD_STS_RUNNABLE);
			prev->be_blocking = false;
		}

		if (next->switch_in != NULL) {
			next->switch_in(next);
		}
		set_thread_status(next, THREAD_STS_RUNNING);

		ctl->curr_obj = next;
		release_schedule_lock(pcpu_id, rflag);
		arch_switch_to(&prev->host_sp, &next->host_sp);
	} else {
		release_schedule_lock(pcpu_id, rflag);
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
		make_reschedule_request(pcpu_id);
		obj->be_blocking = true;
	} else {
		set_thread_status(obj, THREAD_STS_BLOCKED);
	}
	release_schedule_lock(pcpu_id, rflag);
}

void sleep_thread_sync(struct thread_object *obj)
{
	sleep_thread(obj);
	while (!is_blocked(obj)) {
		asm_pause();
	}
}

void wake_thread(struct thread_object *obj)
{
	uint16_t pcpu_id = obj->pcpu_id;
	struct acrn_scheduler *scheduler;
	uint64_t rflag;

	obtain_schedule_lock(pcpu_id, &rflag);
	if (is_blocked(obj) || obj->be_blocking) {
		scheduler = get_scheduler(pcpu_id);
		if (scheduler->wake != NULL) {
			scheduler->wake(obj);
		}
		if (is_blocked(obj)) {
			set_thread_status(obj, THREAD_STS_RUNNABLE);
			make_reschedule_request(pcpu_id);
		}
		obj->be_blocking = false;
	}
	release_schedule_lock(pcpu_id, rflag);
}

void yield_current(void)
{
	make_reschedule_request(get_pcpu_id());
}

void run_thread(struct thread_object *obj)
{
	uint64_t rflag;

	obtain_schedule_lock(obj->pcpu_id, &rflag);
	get_cpu_var(sched_ctl).curr_obj = obj;
	set_thread_status(obj, THREAD_STS_RUNNING);
	release_schedule_lock(obj->pcpu_id, rflag);

	if (obj->thread_entry != NULL) {
		obj->thread_entry(obj);
	}
}
