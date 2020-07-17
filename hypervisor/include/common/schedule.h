/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H
#include <spinlock.h>
#include <list.h>
#include <timer.h>

#define	NEED_RESCHEDULE		(1U)

#define DEL_MODE_NMI		(1U)
#define DEL_MODE_IPI		(2U)

#define THREAD_DATA_SIZE	(256U)

enum thread_object_state {
	THREAD_STS_RUNNING = 1,
	THREAD_STS_RUNNABLE,
	THREAD_STS_BLOCKED
};

enum sched_notify_mode {
	SCHED_NOTIFY_NMI,
	SCHED_NOTIFY_IPI
};

struct thread_object;
typedef void (*thread_entry_t)(struct thread_object *obj);
typedef void (*switch_t)(struct thread_object *obj);
struct thread_object {
	char name[16];
	uint16_t pcpu_id;
	struct sched_control *sched_ctl;
	thread_entry_t thread_entry;
	volatile enum thread_object_state status;
	bool be_blocking;
	enum sched_notify_mode notify_mode;

	uint64_t host_sp;
	switch_t switch_out;
	switch_t switch_in;

	uint8_t data[THREAD_DATA_SIZE];
};

struct sched_control {
	uint16_t pcpu_id;
	uint64_t flags;
	struct thread_object *curr_obj;
	spinlock_t scheduler_lock;	/* to protect sched_control and thread_object */
	struct acrn_scheduler *scheduler;
	void *priv;
};

#define SCHEDULER_MAX_NUMBER 4U
struct acrn_scheduler {
	char name[16];

	/* init scheduler */
	int32_t	(*init)(struct sched_control *ctl);
	/* init private data of scheduler */
	void	(*init_data)(struct thread_object *obj);
	/* pick the next thread object */
	struct thread_object* (*pick_next)(struct sched_control *ctl);
	/* put thread object into sleep */
	void	(*sleep)(struct thread_object *obj);
	/* wake up thread object from sleep status */
	void	(*wake)(struct thread_object *obj);
	/* yield current thread object */
	void	(*yield)(struct sched_control *ctl);
	/* prioritize the thread object */
	void	(*prioritize)(struct thread_object *obj);
	/* deinit private data of scheduler */
	void	(*deinit_data)(struct thread_object *obj);
	/* deinit scheduler */
	void	(*deinit)(struct sched_control *ctl);
};
extern struct acrn_scheduler sched_noop;
extern struct acrn_scheduler sched_iorr;

struct sched_noop_control {
	struct thread_object *noop_thread_obj;
};

struct sched_iorr_control {
	struct list_head runqueue;
	struct hv_timer tick_timer;
};

extern struct acrn_scheduler sched_bvt;
struct sched_bvt_control {
	struct list_head runqueue;
	struct hv_timer tick_timer;
};

bool is_idle_thread(const struct thread_object *obj);
uint16_t sched_get_pcpuid(const struct thread_object *obj);
struct thread_object *sched_get_current(uint16_t pcpu_id);

void init_sched(uint16_t pcpu_id);
void deinit_sched(uint16_t pcpu_id);
void obtain_schedule_lock(uint16_t pcpu_id, uint64_t *rflag);
void release_schedule_lock(uint16_t pcpu_id, uint64_t rflag);

void init_thread_data(struct thread_object *obj);
void deinit_thread_data(struct thread_object *obj);

void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode);
bool need_reschedule(uint16_t pcpu_id);

void run_thread(struct thread_object *obj);
void sleep_thread(struct thread_object *obj);
void sleep_thread_sync(struct thread_object *obj);
void wake_thread(struct thread_object *obj);
void yield_current(void);
void schedule(void);

void arch_switch_to(void *prev_sp, void *next_sp);
void run_idle_thread(void);
#endif /* SCHEDULE_H */

