/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <list.h>
#include <per_cpu.h>
#include <schedule.h>

struct sched_prio_data {
	/* keep list as the first item */
	struct list_head list;
	int priority;
};

static int sched_prio_init(struct sched_control *ctl)
{
	struct sched_prio_control *prio_ctl = &per_cpu(sched_prio_ctl, ctl->pcpu_id);

	ASSERT(ctl->pcpu_id == get_pcpu_id(), "Init scheduler on wrong CPU!");

	ctl->priv = prio_ctl;
	INIT_LIST_HEAD(&prio_ctl->prio_queue);

	return 0;
}

static void sched_prio_init_data(struct thread_object *obj, struct sched_params *params)
{
	struct sched_prio_data *data;

	data = (struct sched_prio_data *)obj->data;
	INIT_LIST_HEAD(&data->list);
	data->priority = params->prio;
}

static struct thread_object *sched_prio_pick_next(struct sched_control *ctl)
{
	struct sched_prio_control *prio_ctl = (struct sched_prio_control *)ctl->priv;
	struct thread_object *next = NULL;

	if (!list_empty(&prio_ctl->prio_queue)) {
		next = get_first_item(&prio_ctl->prio_queue, struct thread_object, data);
	} else {
		next = &get_cpu_var(idle);
	}

	return next;
}

static void prio_queue_add(struct thread_object *obj)
{
	struct sched_prio_control *prio_ctl =
		(struct sched_prio_control *)obj->sched_ctl->priv;
	struct sched_prio_data *data = (struct sched_prio_data *)obj->data;
	struct sched_prio_data *iter_data;
	struct list_head *pos;

	if (list_empty(&prio_ctl->prio_queue)) {
		list_add(&data->list, &prio_ctl->prio_queue);
	} else {
		list_for_each(pos, &prio_ctl->prio_queue) {
			iter_data = container_of(pos, struct sched_prio_data, list);
			if (iter_data->priority < data->priority) {
				list_add_node(&data->list, pos->prev, pos);
				break;
			}
		}
		if (list_empty(&data->list)) {
			list_add_tail(&data->list, &prio_ctl->prio_queue);
		}
	}
}

static void prio_queue_remove(struct thread_object *obj)
{
	struct sched_prio_data *data = (struct sched_prio_data *)obj->data;

	list_del_init(&data->list);
}

static void sched_prio_sleep(struct thread_object *obj)
{
	prio_queue_remove(obj);
}

static void sched_prio_wake(struct thread_object *obj)
{
	prio_queue_add(obj);
}

struct acrn_scheduler sched_prio = {
	.name		= "sched_prio",
	.init		= sched_prio_init,
	.init_data	= sched_prio_init_data,
	.pick_next	= sched_prio_pick_next,
	.sleep		= sched_prio_sleep,
	.wake		= sched_prio_wake,
};
