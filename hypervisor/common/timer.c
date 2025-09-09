/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <per_cpu.h>
#include <asm/io.h>
#include <asm/msr.h>
#include <asm/apicreg.h>
#include <asm/cpuid.h>
#include <asm/cpu_caps.h>
#include <softirq.h>
#include <trace.h>
#include <asm/irq.h>
#include <ticks.h>
#include <timer.h>

#define MAX_TIMER_ACTIONS	32U

bool timer_expired(const struct hv_timer *timer, uint64_t now, uint64_t *delta)
{
	bool ret = true;
	uint64_t delt = 0UL;

	if  ((timer->timeout != 0UL) && (now < timer->timeout)) {
		ret = false;
		delt = timer->timeout - now;
	}

	if (delta != NULL) {
		*delta = delt;
	}

	return ret;
}

bool timer_is_started(const struct hv_timer *timer)
{
	return (!list_empty(&timer->node));
}

static void run_timer(const struct hv_timer *timer)
{
	/* deadline = 0 means stop timer, we should skip */
	if ((timer->func != NULL) && (timer->timeout != 0UL)) {
		timer->func(timer->priv_data);
	}

	TRACE_2L(TRACE_TIMER_ACTION_PCKUP, timer->timeout, 0UL);
}

static inline void update_physical_timer(struct per_cpu_timers *cpu_timer)
{
	struct hv_timer *timer = NULL;

	/* find the next event timer */
	if (!list_empty(&cpu_timer->timer_list)) {
		timer = container_of((&cpu_timer->timer_list)->next,
			struct hv_timer, node);

		/* it is okay to program a expired time */
		arch_set_timer_count(timer->timeout);
	}
}

/*
 * return true if we add the timer on the timer_list head
 */
static bool local_add_timer(struct per_cpu_timers *cpu_timer,
			struct hv_timer *timer)
{
	struct list_head *pos, *prev;
	struct hv_timer *tmp;
	uint64_t tsc = timer->timeout;

	prev = &cpu_timer->timer_list;
	list_for_each(pos, &cpu_timer->timer_list) {
		tmp = container_of(pos, struct hv_timer, node);
		if (tmp->timeout < tsc) {
			prev = &tmp->node;
		} else {
			break;
		}
	}

	list_add(&timer->node, prev);

	return (prev == &cpu_timer->timer_list);
}

int32_t add_timer(struct hv_timer *timer)
{
	struct per_cpu_timers *cpu_timer;
	uint16_t pcpu_id;
	int32_t ret = 0;
	uint64_t rflags;

	if ((timer == NULL) || (timer->func == NULL) || (timer->timeout == 0UL)) {
		ret = -EINVAL;
	} else {
		ASSERT(list_empty(&timer->node), "add timer again!\n");

		/* limit minimal periodic timer cycle period */
		if (timer->mode == TICK_MODE_PERIODIC) {
			timer->period_in_cycle = max(timer->period_in_cycle, us_to_ticks(MIN_TIMER_PERIOD_US));
		}

		pcpu_id  = get_pcpu_id();
		cpu_timer = &per_cpu(cpu_timers, pcpu_id);

		CPU_INT_ALL_DISABLE(&rflags);
		/* update the physical timer if we're on the timer_list head */
		if (local_add_timer(cpu_timer, timer)) {
			update_physical_timer(cpu_timer);
		}
		CPU_INT_ALL_RESTORE(rflags);

		TRACE_2L(TRACE_TIMER_ACTION_ADDED, timer->timeout, 0UL);
	}

	return ret;

}

void initialize_timer(struct hv_timer *timer,
			timer_handle_t func, void *priv_data,
			uint64_t timeout, uint64_t period_in_cycle)
{
	if (timer != NULL) {
		timer->func = func;
		timer->priv_data = priv_data;
		timer->timeout = timeout;
		if (period_in_cycle > 0UL) {
			timer->mode = TICK_MODE_PERIODIC;
			timer->period_in_cycle = period_in_cycle;
		} else {
			timer->mode = TICK_MODE_ONESHOT;
			timer->period_in_cycle = 0UL;
		}
		INIT_LIST_HEAD(&timer->node);
	}
}

void update_timer(struct hv_timer *timer, uint64_t timeout, uint64_t period)
{
	if (timer != NULL) {
		timer->timeout = timeout;
		if (period > 0UL) {
			timer->mode = TICK_MODE_PERIODIC;
			timer->period_in_cycle = period;
		} else {
			timer->mode = TICK_MODE_ONESHOT;
			timer->period_in_cycle = 0UL;
		}
	}
}

void del_timer(struct hv_timer *timer)
{
	uint64_t rflags;

	CPU_INT_ALL_DISABLE(&rflags);
	if ((timer != NULL) && !list_empty(&timer->node)) {
		list_del_init(&timer->node);
	}
	CPU_INT_ALL_RESTORE(rflags);
}

static void init_percpu_timer(uint16_t pcpu_id)
{
	struct per_cpu_timers *cpu_timer;

	cpu_timer = &per_cpu(cpu_timers, pcpu_id);
	INIT_LIST_HEAD(&cpu_timer->timer_list);
}

static void timer_softirq(uint16_t pcpu_id)
{
	struct per_cpu_timers *cpu_timer;
	struct hv_timer *timer;
	const struct list_head *pos, *n;
	uint32_t tries = MAX_TIMER_ACTIONS;
	uint64_t current_tsc = cpu_ticks();

	/* handle passed timer */
	cpu_timer = &per_cpu(cpu_timers, pcpu_id);

	/* This is to make sure we are not blocked due to delay inside func()
	 * force to exit irq handler after we serviced >31 timers
	 * caller used to local_add_timer() for periodic timer, if there is a delay
	 * inside func(), it will infinitely loop here, because new added timer
	 * already passed due to previously func()'s delay.
	 */
	list_for_each_safe(pos, n, &cpu_timer->timer_list) {
		timer = container_of(pos, struct hv_timer, node);
		/* timer expried */
		tries--;
		if ((timer->timeout <= current_tsc) && (tries != 0U)) {
			del_timer(timer);

			run_timer(timer);

			if (timer->mode == TICK_MODE_PERIODIC) {
				/* update periodic timer fire tsc */
				timer->timeout += timer->period_in_cycle;
				(void)local_add_timer(cpu_timer, timer);
			} else {
				timer->timeout = 0UL;
			}
		} else {
			break;
		}
	}

	/* update nearest timer */
	update_physical_timer(cpu_timer);
}

void timer_init(void)
{
	uint16_t pcpu_id = get_pcpu_id();

	init_percpu_timer(pcpu_id);

	if (pcpu_id == BSP_CPU_ID) {
		register_softirq(SOFTIRQ_TIMER, timer_softirq);
	}

	arch_init_timer();
}
