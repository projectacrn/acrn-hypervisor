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

struct thread_object;
typedef void (*thread_entry_t)(struct thread_object *obj);
typedef void (*switch_t)(struct thread_object *obj);
struct thread_object {
	char name[16];
	uint64_t host_sp;
	thread_entry_t thread_entry;
	switch_t switch_out;
	switch_t switch_in;
};

struct sched_control {
	uint64_t flags;
	struct thread_object *curr_obj;
	spinlock_t scheduler_lock;	/* to protect sched_control and thread_object */

	struct thread_object *thread_obj;
};

void init_scheduler(void);
void switch_to_idle(thread_entry_t idle_thread);
void get_schedule_lock(uint16_t pcpu_id);
void release_schedule_lock(uint16_t pcpu_id);

void insert_thread_obj(struct thread_object *obj, uint16_t pcpu_id);
void remove_thread_obj(struct thread_object *obj, uint16_t pcpu_id);

void make_reschedule_request(uint16_t pcpu_id, uint16_t delmode);
bool need_reschedule(uint16_t pcpu_id);

void schedule(void);
void run_sched_thread(struct thread_object *obj);

void arch_switch_to(void *prev_sp, void *next_sp);
#endif /* SCHEDULE_H */

