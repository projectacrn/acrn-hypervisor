/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <list.h>
#include <per_cpu.h>
#include <schedule.h>

struct sched_iorr_data {
	/* keep list as the first item */
	struct list_head list;

	uint64_t slice_cycles;
	uint64_t last_cycles;
	int64_t  left_cycles;
};

int sched_iorr_init(__unused struct sched_control *ctl)
{
	return 0;
}

void sched_iorr_deinit(__unused struct sched_control *ctl)
{
}

void sched_iorr_init_data(__unused struct thread_object *obj)
{
}

static struct thread_object *sched_iorr_pick_next(__unused struct sched_control *ctl)
{
	return NULL;
}

static void sched_iorr_sleep(__unused struct thread_object *obj)
{
}

static void sched_iorr_wake(__unused struct thread_object *obj)
{
}

struct acrn_scheduler sched_iorr = {
	.name		= "sched_iorr",
	.init		= sched_iorr_init,
	.init_data	= sched_iorr_init_data,
	.pick_next	= sched_iorr_pick_next,
	.sleep		= sched_iorr_sleep,
	.wake		= sched_iorr_wake,
	.deinit		= sched_iorr_deinit,
};
