/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TIMER_H
#define TIMER_H

typedef void (*timer_handle_t)(void *data);

enum tick_mode {
	TICK_MODE_ONESHOT = 0,
	TICK_MODE_PERIODIC,
};

struct per_cpu_timers {
	struct list_head timer_list;	/* it's for runtime active timer list */
};

struct hv_timer {
	struct list_head node;		/* link all timers */
	int mode;			/* timer mode: one-shot or periodic */
	uint64_t fire_tsc;		/* tsc deadline to interrupt */
	uint64_t period_in_cycle;	/* period of the periodic timer in unit of TSC cycles */
	timer_handle_t func;		/* callback if time reached */
	void *priv_data;		/* func private data */
};

/*
 * Don't initialize a timer twice if it has been add to the timer list
 * after call add_timer. If u want, delete the timer from the list first.
 */
static inline void initialize_timer(struct hv_timer *timer,
				timer_handle_t func,
				void *priv_data,
				uint64_t fire_tsc,
				int mode,
				uint64_t period_in_cycle)
{
	if (timer != NULL) {
		timer->func = func;
		timer->priv_data = priv_data;
		timer->fire_tsc = fire_tsc;
		timer->mode = mode;
		timer->period_in_cycle = period_in_cycle;
		INIT_LIST_HEAD(&timer->node);
	}
}

static inline bool timer_expired(const struct hv_timer *timer)
{
	return ((timer->fire_tsc == 0UL) || (rdtsc() >= timer->fire_tsc));
}

/*
 * Don't call add_timer/del_timer in the timer callback function.
 */
int add_timer(struct hv_timer *timer);
void del_timer(struct hv_timer *timer);

void timer_init(void);
void check_tsc(void);
void calibrate_tsc(void);

#endif /* TIMER_H */
