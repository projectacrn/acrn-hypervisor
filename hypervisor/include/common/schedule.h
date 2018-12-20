/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H

#define	NEED_RESCHEDULE		(1U)
#define	NEED_OFFLINE		(2U)

struct sched_object {
	struct list_head run_list;
};

struct sched_context {
	spinlock_t runqueue_lock;
	struct list_head runqueue;
	uint64_t flags;
	struct acrn_vcpu *curr_vcpu;
	spinlock_t scheduler_lock;
};

void init_scheduler(void);
void get_schedule_lock(uint16_t pcpu_id);
void release_schedule_lock(uint16_t pcpu_id);

void set_pcpu_used(uint16_t pcpu_id);
uint16_t allocate_pcpu(void);
void free_pcpu(uint16_t pcpu_id);

void add_to_cpu_runqueue(struct sched_object *obj, uint16_t pcpu_id);
void remove_from_cpu_runqueue(struct sched_object *obj, uint16_t pcpu_id);

void default_idle(void);

void make_reschedule_request(uint16_t pcpu_id);
int32_t need_reschedule(uint16_t pcpu_id);
void make_pcpu_offline(uint16_t pcpu_id);
int32_t need_offline(uint16_t pcpu_id);

void schedule(void);

void vcpu_thread(struct acrn_vcpu *vcpu);
#endif /* SCHEDULE_H */

