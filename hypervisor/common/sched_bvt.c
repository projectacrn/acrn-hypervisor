/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <list.h>
#include <per_cpu.h>
#include <schedule.h>
#include <ticks.h>

#define BVT_MCU_MS		1U
/* context switch allowance */
#define BVT_CSA_MCU		5U

/*
 * limit the weight range to [1, 128]. It's enough to allocate CPU resources
 * for different types of vCPUs
 */
#define BVT_WEIGHT_MIN		1U
#define BVT_WEIGHT_MAX		128U

/*
 * the VT (Virtual Time) ratio is proportional to 1 / weight and making the VT
 * ratio an integer will ease translation between virtual time and physical
 * time.
 * Max (theoretical VT ratio - actual VT ratio) is
 *   1 (< 1 because of integer round down).
 * The minimum total VT ratios of VCPUs (at least two) is
 *   2 * 8 (Min per-vcpu VT ratio)
 * So the max VT ratio share error is about 1/16.
 * To reduce it, we can enlarge the BVT_VT_RATIO_MIN.
 * However increasing VT ratio will reduce the total time needed to overflow
 * AVT. AVT is of type int64_t. The max VT ratio is 1024. MCU is 1 ms.
 * So the time to overflow AVT is about:
 *   2^63  / (1024 * 1000) s, i.e. ~= 9 * 10^12(s) ~= 10^8 day
 * It's so large that we can ignore the AVT overflow case.
 */
#define BVT_VT_RATIO_MIN	8U
#define BVT_VT_RATIO_MAX	(BVT_WEIGHT_MAX * BVT_VT_RATIO_MIN / BVT_WEIGHT_MIN)

struct sched_bvt_data {
	/* keep list as the first item */
	struct list_head list;
	/* minimum charging unit in cycles */
	uint64_t mcu;
	/* a thread receives a share of cpu in proportion to its weight */
	uint8_t weight;
	/* virtual time advance variable, proportional to 1 / weight */
	uint64_t vt_ratio;
	bool warp_on;
	int32_t warp_value;
	uint32_t warp_limit;
	uint32_t unwarp_period;
	/* actual virtual time in units of mcu */
	int64_t avt;
	/* effective virtual time in units of mcu */
	int64_t evt;
	uint64_t residual;

	uint64_t start_tsc;
};

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 */
static bool is_inqueue(struct thread_object *obj)
{
	struct sched_bvt_data *data = (struct sched_bvt_data *)obj->data;
	return !list_empty(&data->list);
}

/*
 * @pre bvt_ctl != NULL
 */
static void update_svt(struct sched_bvt_control *bvt_ctl)
{
	struct sched_bvt_data *obj_data;
	struct thread_object *tmp_obj;

	if (!list_empty(&bvt_ctl->runqueue)) {
		tmp_obj = get_first_item(&bvt_ctl->runqueue, struct thread_object, data);
		obj_data = (struct sched_bvt_data *)tmp_obj->data;
		bvt_ctl->svt = obj_data->avt;
	}
}

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 * @pre obj->sched_ctl != NULL
 * @pre obj->sched_ctl->priv != NULL
 */
static void runqueue_add(struct thread_object *obj)
{
	struct sched_bvt_control *bvt_ctl =
		(struct sched_bvt_control *)obj->sched_ctl->priv;
	struct sched_bvt_data *data = (struct sched_bvt_data *)obj->data;
	struct list_head *pos;
	struct thread_object *iter_obj;
	struct sched_bvt_data *iter_data;

	/*
	 * the earliest evt has highest priority,
	 * the runqueue is ordered by priority.
	 */

	if (list_empty(&bvt_ctl->runqueue)) {
		list_add(&data->list, &bvt_ctl->runqueue);
	} else {
		list_for_each(pos, &bvt_ctl->runqueue) {
			iter_obj = container_of(pos, struct thread_object, data);
			iter_data = (struct sched_bvt_data *)iter_obj->data;
			if (iter_data->evt > data->evt) {
				list_add_node(&data->list, pos->prev, pos);
				break;
			}
		}
		if (!is_inqueue(obj)) {
			list_add_tail(&data->list, &bvt_ctl->runqueue);
		}
	}
}

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 */
static void runqueue_remove(struct thread_object *obj)
{
	struct sched_bvt_data *data = (struct sched_bvt_data *)obj->data;

	list_del_init(&data->list);
}

/*
 * @brief Get the SVT (scheduler virtual time) which indicates the
 * minimum AVT of any runnable threads.
 * @pre obj != NULL
 * @pre obj->data != NULL
 * @pre obj->sched_ctl != NULL
 * @pre obj->sched_ctl->priv != NULL
 */

static int64_t get_svt(struct thread_object *obj)
{
	struct sched_bvt_control *bvt_ctl = (struct sched_bvt_control *)obj->sched_ctl->priv;

	return bvt_ctl->svt;
}

static void sched_tick_handler(void *param)
{
	struct sched_control  *ctl = (struct sched_control *)param;
	struct sched_bvt_control *bvt_ctl = (struct sched_bvt_control *)ctl->priv;
	struct thread_object *current;
	uint16_t pcpu_id = get_pcpu_id();
	uint64_t rflags;

	obtain_schedule_lock(pcpu_id, &rflags);
	current = ctl->curr_obj;

	if (current != NULL ) {
		/* only non-idle thread need to consume run_countdown */
		if (!is_idle_thread(current)) {
			make_reschedule_request(pcpu_id);
		} else {
			if (!list_empty(&bvt_ctl->runqueue)) {
				make_reschedule_request(pcpu_id);
			}
		}
	}
	release_schedule_lock(pcpu_id, rflags);
}

/*
 *@pre: ctl->pcpu_id == get_pcpu_id()
 */
static int sched_bvt_init(struct sched_control *ctl)
{
	struct sched_bvt_control *bvt_ctl = &per_cpu(sched_bvt_ctl, ctl->pcpu_id);
	int ret = 0;

	ASSERT(ctl->pcpu_id == get_pcpu_id(), "Init scheduler on wrong CPU!");

	ctl->priv = bvt_ctl;
	INIT_LIST_HEAD(&bvt_ctl->runqueue);

	/* The tick_timer is periodically */
	initialize_timer(&bvt_ctl->tick_timer, sched_tick_handler, ctl, 0, 0);

	return ret;
}

static void sched_bvt_deinit(struct sched_control *ctl)
{
	struct sched_bvt_control *bvt_ctl = (struct sched_bvt_control *)ctl->priv;
	del_timer(&bvt_ctl->tick_timer);
}

static void sched_bvt_init_data(struct thread_object *obj, struct sched_params * params)
{
	struct sched_bvt_data *data;

	data = (struct sched_bvt_data *)obj->data;
	INIT_LIST_HEAD(&data->list);
	data->mcu = BVT_MCU_MS * TICKS_PER_MS;
	data->weight = clamp(params->bvt_weight, BVT_WEIGHT_MIN, BVT_WEIGHT_MAX);
	data->warp_value = params->bvt_warp_value;
	data->warp_limit = params->bvt_warp_limit;
	data->unwarp_period = params->bvt_unwarp_period;
	data->warp_on = false;	/* warp disabled by default */
	data->vt_ratio = BVT_VT_RATIO_MAX / data->weight;
	data->residual = 0U;
}

static void sched_bvt_suspend(struct sched_control *ctl)
{
	sched_bvt_deinit(ctl);
}

static uint64_t v2p(uint64_t virt_time, uint64_t ratio)
{
	return (uint64_t)(virt_time / ratio);
}

static uint64_t p2v(uint64_t phy_time, uint64_t ratio)
{
	return (uint64_t)(phy_time * ratio);
}

static void update_vt(struct thread_object *obj)
{
	struct sched_bvt_data *data;
	uint64_t now_tsc = cpu_ticks();
	uint64_t v_delta, delta_mcu = 0U;

	data = (struct sched_bvt_data *)obj->data;

	/* update current thread's avt and evt */
	if (now_tsc > data->start_tsc) {
		v_delta = p2v(now_tsc - data->start_tsc, data->vt_ratio) + data->residual;
		delta_mcu = (uint64_t)(v_delta / data->mcu);
		data->residual = v_delta % data->mcu;
	}
	data->avt += delta_mcu;
	/* TODO: evt = avt - (warp ? warpback : 0U) */
	data->evt = data->avt;

	if (is_inqueue(obj)) {
		runqueue_remove(obj);
		runqueue_add(obj);
	}
}

static struct thread_object *sched_bvt_pick_next(struct sched_control *ctl)
{
	struct sched_bvt_control *bvt_ctl = (struct sched_bvt_control *)ctl->priv;
	struct thread_object *first_obj = NULL, *second_obj = NULL;
	struct sched_bvt_data *first_data = NULL, *second_data = NULL;
	struct list_head *first, *sec;
	struct thread_object *next = NULL;
	struct thread_object *current = ctl->curr_obj;
	uint64_t now_tsc = cpu_ticks();
	uint64_t delta_mcu = 0U;
	uint64_t tick_period = BVT_MCU_MS * TICKS_PER_MS;
	uint64_t run_countdown;

	if (!is_idle_thread(current)) {
		update_vt(current);
	}
	/* always align the svt with the avt of the first thread object in runqueue.*/
	update_svt(bvt_ctl);

	del_timer(&bvt_ctl->tick_timer);

	if (!list_empty(&bvt_ctl->runqueue)) {
		first = bvt_ctl->runqueue.next;
		sec = (first->next == &bvt_ctl->runqueue) ? NULL : first->next;

		first_obj = container_of(first, struct thread_object, data);
		first_data = (struct sched_bvt_data *)first_obj->data;

		/* The run_countdown is used to describe how may mcu the next thread
		 * can run for. A one-shot timer is set to expire at
		 * current time + run_countdown. The next thread can run until the
		 * timer interrupts. But when there is only one object
		 * in runqueue, it can run forever. so, no timer is set.
		 */
		if (sec != NULL) {
			second_obj = container_of(sec, struct thread_object, data);
			second_data = (struct sched_bvt_data *)second_obj->data;
			delta_mcu = second_data->evt - first_data->evt;
			run_countdown = v2p(delta_mcu, first_data->vt_ratio) + BVT_CSA_MCU;
		} else {
			run_countdown = UINT64_MAX;
		}
		first_data->start_tsc = now_tsc;
		next = first_obj;
		if (run_countdown != UINT64_MAX) {
			update_timer(&bvt_ctl->tick_timer, cpu_ticks() + run_countdown * tick_period, 0);
			(void)add_timer(&bvt_ctl->tick_timer);
		}
	} else {
		next = &get_cpu_var(idle);
	}

	return next;
}

static void sched_bvt_sleep(struct thread_object *obj)
{
	runqueue_remove(obj);
}

static void sched_bvt_wake(struct thread_object *obj)
{
	struct sched_bvt_data *data;
	int64_t svt, threshold;

	data = (struct sched_bvt_data *)obj->data;
	svt = get_svt(obj);
	threshold = svt - BVT_CSA_MCU;
	/* adjusting AVT for a thread after a long sleep */
	data->avt = (data->avt > threshold) ? data->avt : svt;
	/* TODO: evt = avt - (warp ? warpback : 0U) */
	data->evt = data->avt;
	/* add to runqueue in order */
	runqueue_add(obj);

}

struct acrn_scheduler sched_bvt = {
	.name		= "sched_bvt",
	.init		= sched_bvt_init,
	.init_data	= sched_bvt_init_data,
	.pick_next	= sched_bvt_pick_next,
	.sleep		= sched_bvt_sleep,
	.wake		= sched_bvt_wake,
	.deinit		= sched_bvt_deinit,
	/* Now suspend is just to do del_timer and add_timer will be delayed to
	 * shedule after resume.
	 * So no need to add .resume now.
	 */
	.suspend	= sched_bvt_suspend,
};
