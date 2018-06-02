/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef	_HV_CORE_SCHEDULE_
#define	_HV_CORE_SCHEDULE_

#define	NEED_RESCHEDULE		(1)
#define	NEED_OFFLINE		(2)

struct sched_context {
	spinlock_t runqueue_lock;
	struct list_head runqueue;
	unsigned long flags;
	struct vcpu *curr_vcpu;
	spinlock_t scheduler_lock;
};

void init_scheduler(void);
void get_schedule_lock(int pcpu_id);
void release_schedule_lock(int pcpu_id);

void set_pcpu_used(int pcpu_id);
int allocate_pcpu(void);
void free_pcpu(int pcpu_id);

void add_vcpu_to_runqueue(struct vcpu *vcpu);
void remove_vcpu_from_runqueue(struct vcpu *vcpu);

void default_idle(void);

void make_reschedule_request(struct vcpu *vcpu);
int need_reschedule(int pcpu_id);
void make_pcpu_offline(int pcpu_id);
int need_offline(int pcpu_id);

void schedule(void);

void vcpu_thread(struct vcpu *vcpu);
#endif

