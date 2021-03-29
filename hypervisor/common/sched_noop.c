/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/per_cpu.h>
#include <schedule.h>

static int32_t sched_noop_init(struct sched_control *ctl)
{
	struct sched_noop_control *noop_ctl = &per_cpu(sched_noop_ctl, ctl->pcpu_id);
	ctl->priv = noop_ctl;

	return 0;
}

static struct thread_object *sched_noop_pick_next(struct sched_control *ctl)
{
	struct sched_noop_control *noop_ctl = (struct sched_noop_control *)ctl->priv;
	struct thread_object *next = NULL;

	if (noop_ctl->noop_thread_obj != NULL) {
		next = noop_ctl->noop_thread_obj;
	} else {
		next = &get_cpu_var(idle);
	}
	return next;
}

static void sched_noop_sleep(struct thread_object *obj)
{
	struct sched_noop_control *noop_ctl = (struct sched_noop_control *)obj->sched_ctl->priv;

	if (noop_ctl->noop_thread_obj == obj) {
		noop_ctl->noop_thread_obj = NULL;
	}
}

static void sched_noop_wake(struct thread_object *obj)
{
	struct sched_noop_control *noop_ctl = (struct sched_noop_control *)obj->sched_ctl->priv;

	if (noop_ctl->noop_thread_obj == NULL) {
		noop_ctl->noop_thread_obj = obj;
	}
}

struct acrn_scheduler sched_noop = {
	.name		= "sched_noop",
	.init		= sched_noop_init,
	.pick_next	= sched_noop_pick_next,
	.sleep		= sched_noop_sleep,
	.wake		= sched_noop_wake,
};
