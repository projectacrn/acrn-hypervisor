/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H
#include <spinlock.h>

#define	NEED_RESCHEDULE		(1U)

#define DEL_MODE_INIT		(1U)
#define DEL_MODE_IPI		(2U)

struct sched_object;
typedef void (*run_thread_t)(struct sched_object *obj);
typedef void (*prepare_switch_t)(struct sched_object *obj);
struct sched_object {
	char name[16];
	struct list_head run_list;
	uint64_t host_sp;
	run_thread_t thread;
	prepare_switch_t prepare_switch_out;
	prepare_switch_t prepare_switch_in;
};

struct sched_context {
	struct list_head runqueue;
	uint64_t flags;
	struct sched_object *curr_obj;
	spinlock_t scheduler_lock;	/* to protect sched_context and sched_object */
};

void init_scheduler(void);
void switch_to_idle(run_thread_t idle_thread);
void get_schedule_lock(uint16_t pcpu_id);
void release_schedule_lock(uint16_t pcpu_id);

void add_to_cpu_runqueue(struct sched_object *obj, uint16_t pcpu_id);
void remove_from_cpu_runqueue(struct sched_object *obj);

void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode);
bool need_reschedule(uint16_t pcpu_id);

void schedule(void);
void run_sched_thread(struct sched_object *obj);

void arch_switch_to(void *prev_sp, void *next_sp);
#endif /* SCHEDULE_H */

