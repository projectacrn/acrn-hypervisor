/*
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <list.h>
#include <per_cpu.h>
#include <schedule.h>
#include <ticks.h>

#define CONFIG_SLICE_MS 10UL
struct sched_iorr_data {
	/* keep list as the first item */
	struct list_head list;

	uint64_t slice_cycles;
	uint64_t last_cycles;
	int64_t  left_cycles;
};

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 */
bool is_inqueue(struct thread_object *obj)
{
	struct sched_iorr_data *data = (struct sched_iorr_data *)obj->data;
	return !list_empty(&data->list);
}

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 * @pre obj->sched_ctl != NULL
 * @pre obj->sched_ctl->priv != NULL
 */
void runqueue_add_head(struct thread_object *obj)
{
	struct sched_iorr_control *iorr_ctl = (struct sched_iorr_control *)obj->sched_ctl->priv;
	struct sched_iorr_data *data = (struct sched_iorr_data *)obj->data;

	if (!is_inqueue(obj)) {
		list_add(&data->list, &iorr_ctl->runqueue);
	}
}

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 * @pre obj->sched_ctl != NULL
 * @pre obj->sched_ctl->priv != NULL
 */
void runqueue_add_tail(struct thread_object *obj)
{
	struct sched_iorr_control *iorr_ctl = (struct sched_iorr_control *)obj->sched_ctl->priv;
	struct sched_iorr_data *data = (struct sched_iorr_data *)obj->data;

	if (!is_inqueue(obj)) {
		list_add_tail(&data->list, &iorr_ctl->runqueue);
	}
}

/*
 * @pre obj != NULL
 * @pre obj->data != NULL
 */
void runqueue_remove(struct thread_object *obj)
{
	struct sched_iorr_data *data = (struct sched_iorr_data *)obj->data;
	list_del_init(&data->list);
}

static void sched_tick_handler(void *param)
{
	struct sched_control  *ctl = (struct sched_control *)param;
	struct sched_iorr_control *iorr_ctl = (struct sched_iorr_control *)ctl->priv;
	struct sched_iorr_data *data;
	struct thread_object *current;
	uint16_t pcpu_id = get_pcpu_id();
	uint64_t now = cpu_ticks();
	uint64_t rflags;

	obtain_schedule_lock(pcpu_id, &rflags);
	current = ctl->curr_obj;
	/* If no vCPU start scheduling, ignore this tick */
	if (current != NULL ) {
		if (!(is_idle_thread(current) && list_empty(&iorr_ctl->runqueue))) {
			data = (struct sched_iorr_data *)current->data;
			/* consume the left_cycles of current thread_object if it is not idle */
			if (!is_idle_thread(current)) {
				data->left_cycles -= now - data->last_cycles;
				data->last_cycles = now;
			}
			/* make reschedule request if current ran out of its cycles */
			if (is_idle_thread(current) || data->left_cycles <= 0) {
				make_reschedule_request(pcpu_id);
			}
		}
	}
	release_schedule_lock(pcpu_id, rflags);
}

int sched_iorr_add_timer(struct sched_control *ctl)
{
	struct sched_iorr_control *iorr_ctl = &per_cpu(sched_iorr_ctl, ctl->pcpu_id);
	uint64_t tick_period = TICKS_PER_MS;
	int ret = 0;

	/* The tick_timer is periodically */
	initialize_timer(&iorr_ctl->tick_timer, sched_tick_handler, ctl,
			cpu_ticks() + tick_period, tick_period);

	if (add_timer(&iorr_ctl->tick_timer) < 0) {
		pr_err("Failed to add schedule tick timer!");
		ret = -1;
	}
	return ret;
}

static void sched_iorr_del_timer(struct sched_control *ctl)
{
	struct sched_iorr_control *iorr_ctl = (struct sched_iorr_control *)ctl->priv;
	del_timer(&iorr_ctl->tick_timer);
}

/*
 * @pre ctl->pcpu_id == get_pcpu_id()
 */
int sched_iorr_init(struct sched_control *ctl)
{
	struct sched_iorr_control *iorr_ctl = &per_cpu(sched_iorr_ctl, ctl->pcpu_id);

	ASSERT(get_pcpu_id() == ctl->pcpu_id, "Init scheduler on wrong CPU!");

	ctl->priv = iorr_ctl;
	INIT_LIST_HEAD(&iorr_ctl->runqueue);
	return sched_iorr_add_timer(ctl);
}

void sched_iorr_deinit(struct sched_control *ctl)
{
	sched_iorr_del_timer(ctl);
}

static void sched_iorr_suspend(struct sched_control *ctl)
{
	sched_iorr_del_timer(ctl);
}

static void sched_iorr_resume(struct sched_control *ctl)
{
	sched_iorr_add_timer(ctl);
}

void sched_iorr_init_data(struct thread_object *obj, __unused struct sched_params * params)
{
	struct sched_iorr_data *data;

	data = (struct sched_iorr_data *)obj->data;
	INIT_LIST_HEAD(&data->list);
	data->left_cycles = data->slice_cycles = CONFIG_SLICE_MS * TICKS_PER_MS;
}

static struct thread_object *sched_iorr_pick_next(struct sched_control *ctl)
{
	struct sched_iorr_control *iorr_ctl = (struct sched_iorr_control *)ctl->priv;
	struct thread_object *next = NULL;
	struct thread_object *current = NULL;
	struct sched_iorr_data *data;
	uint64_t now = cpu_ticks();

	current = ctl->curr_obj;
	data = (struct sched_iorr_data *)current->data;
	/* Ignore the idle object, inactive objects */
	if (!is_idle_thread(current) && is_inqueue(current)) {
		data->left_cycles -= now - data->last_cycles;
		if (data->left_cycles <= 0) {
			/*  replenish thread_object with slice_cycles */
			data->left_cycles += data->slice_cycles;
		}
		/* move the thread_object to tail */
		runqueue_remove(current);
		runqueue_add_tail(current);
	}

	/*
	 * Pick the next runnable sched object
	 * 1) get the first item in runqueue firstly
	 * 2) if object picked has no time_cycles, replenish it pick this one
	 * 3) At least take one idle sched object if we have no runnable one after step 1) and 2)
	 */
	if (!list_empty(&iorr_ctl->runqueue)) {
		next = get_first_item(&iorr_ctl->runqueue, struct thread_object, data);
		data = (struct sched_iorr_data *)next->data;
		data->last_cycles = now;
		while (data->left_cycles <= 0) {
			data->left_cycles += data->slice_cycles;
		}
	} else {
		next = &get_cpu_var(idle);
	}

	return next;
}

static void sched_iorr_sleep(struct thread_object *obj)
{
	runqueue_remove(obj);
}

static void sched_iorr_wake(struct thread_object *obj)
{
	runqueue_add_head(obj);
}

struct acrn_scheduler sched_iorr = {
	.name		= "sched_iorr",
	.init		= sched_iorr_init,
	.init_data	= sched_iorr_init_data,
	.pick_next	= sched_iorr_pick_next,
	.sleep		= sched_iorr_sleep,
	.wake		= sched_iorr_wake,
	.deinit		= sched_iorr_deinit,
	.suspend	= sched_iorr_suspend,
	.resume		= sched_iorr_resume,
};
