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

static void sched_tick_handler(__unused void *param)
{
}

/*
 *@pre: ctl->pcpu_id == get_pcpu_id()
 */
static int sched_bvt_init(struct sched_control *ctl)
{
	struct sched_bvt_control *bvt_ctl = &per_cpu(sched_bvt_ctl, ctl->pcpu_id);
	uint64_t tick_period = BVT_MCU_MS * CYCLES_PER_MS;
	int ret = 0;

	ASSERT(ctl->pcpu_id == get_pcpu_id(), "Init scheduler on wrong CPU!");

	ctl->priv = bvt_ctl;
	INIT_LIST_HEAD(&bvt_ctl->runqueue);

	/* The tick_timer is periodically */
	initialize_timer(&bvt_ctl->tick_timer, sched_tick_handler, ctl,
			rdtsc() + tick_period, TICK_MODE_PERIODIC, tick_period);

	if (add_timer(&bvt_ctl->tick_timer) < 0) {
		pr_err("Failed to add schedule tick timer!");
		ret = -1;
	}

	return ret;
}

static void sched_bvt_deinit(struct sched_control *ctl)
{
	struct sched_bvt_control *bvt_ctl = (struct sched_bvt_control *)ctl->priv;
	del_timer(&bvt_ctl->tick_timer);
}

static void sched_bvt_init_data(struct thread_object *obj)
{
	struct sched_bvt_data *data;

	data = (struct sched_bvt_data *)obj->data;
	INIT_LIST_HEAD(&data->list);
	data->mcu = BVT_MCU_MS * CYCLES_PER_MS;
	/* TODO: virtual time advance ratio should be proportional to weight. */
	data->vt_ratio = 1U;
	data->run_countdown = BVT_CSA_MCU;
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
