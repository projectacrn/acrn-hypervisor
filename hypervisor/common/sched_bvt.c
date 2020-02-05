/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <list.h>
#include <per_cpu.h>
#include <schedule.h>

#define BVT_MCU_MS	1U
/* context switch allowance */
#define BVT_CSA_MCU 5U
struct sched_bvt_data {
	/* keep list as the first item */
	struct list_head list;
	/* minimum charging unit in cycles */
	uint64_t mcu;
	/* a thread receives a share of cpu in proportion to its weight */
	uint16_t weight;
	/* virtual time advance variable, proportional to 1 / weight */
	uint64_t vt_ratio;
	/* the count down number of mcu until reschedule should take place */
	uint64_t run_countdown;
	/* actual virtual time in units of mcu */
	int64_t avt;
	/* effective virtual time in units of mcu */
	int64_t evt;

	uint64_t start_tsc;
};

static int sched_bvt_init(__unused struct sched_control *ctl)
{
	return 0;
}

static void sched_bvt_deinit(__unused struct sched_control *ctl)
{
}

static void sched_bvt_init_data(__unused struct thread_object *obj)
{
}

static struct thread_object *sched_bvt_pick_next(__unused struct sched_control *ctl)
{
	return NULL;
}

static void sched_bvt_sleep(__unused struct thread_object *obj)
{
}

static void sched_bvt_wake(__unused struct thread_object *obj)
{
}

struct acrn_scheduler sched_bvt = {
	.name		= "sched_bvt",
	.init		= sched_bvt_init,
	.init_data	= sched_bvt_init_data,
	.pick_next	= sched_bvt_pick_next,
	.sleep		= sched_bvt_sleep,
	.wake		= sched_bvt_wake,
	.deinit		= sched_bvt_deinit,
};
