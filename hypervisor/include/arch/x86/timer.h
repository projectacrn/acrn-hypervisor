/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TIMER_H
#define TIMER_H

/**
 * @brief Timer
 *
 * @defgroup timer ACRN Timer
 * @{
 */

typedef void (*timer_handle_t)(void *data);

/**
 * @brief Definition of timer tick mode
 */
enum tick_mode {
	TICK_MODE_ONESHOT = 0,	/**< one-shot mode */
	TICK_MODE_PERIODIC,	/**< periodic mode */
};

/**
 * @brief Definition of timers for per-cpu
 */
struct per_cpu_timers {
	struct list_head timer_list;	/**< it's for runtime active timer list */
};

/**
 * @brief Definition of timer
 */
struct hv_timer {
	struct list_head node;		/**< link all timers */
	enum tick_mode mode;		/**< timer mode: one-shot or periodic */
	uint64_t fire_tsc;		/**< tsc deadline to interrupt */
	uint64_t period_in_cycle;	/**< period of the periodic timer in unit of TSC cycles */
	timer_handle_t func;		/**< callback if time reached */
	void *priv_data;		/**< func private data */
};

/* External Interfaces */

/**
 * @brief Initialize a timer structure.
 *
 * @param[in] timer Pointer to timer.
 * @param[in] func irq callback if time reached.
 * @param[in] priv_data func private data.
 * @param[in] fire_tsc tsc deadline to interrupt.
 * @param[in] mode timer mode.
 * @param[in] period_in_cycle period of the periodic timer in unit of TSC cycles.
 *
 * @remark Don't initialize a timer twice if it has been added to the timer list
 *         after calling add_timer. If you want to, delete the timer from the list first.
 *
 * @return None
 */
static inline void initialize_timer(struct hv_timer *timer,
				timer_handle_t func, void *priv_data,
				uint64_t fire_tsc, int32_t mode, uint64_t period_in_cycle)
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

/**
 * @brief Check a timer whether expired.
 *
 * @param[in] timer Pointer to timer.
 *
 * @retval true if the timer is expired, false otherwise.
 */
static inline bool timer_expired(const struct hv_timer *timer)
{
	return ((timer->fire_tsc == 0UL) || (rdtsc() >= timer->fire_tsc));
}

/**
 * @brief Check a timer whether in timer list.
 *
 * @param[in] timer Pointer to timer.
 *
 * @retval true if the timer is in timer list, false otherwise.
 */
static inline bool timer_is_started(const struct hv_timer *timer)
{
	return (!list_empty(&timer->node));
}

/**
 * @brief Add a timer.
 *
 * @param[in] timer Pointer to timer.
 *
 * @retval 0 on success
 * @retval -EINVAL timer has an invalid value
 *
 * @remark Don't call it in the timer callback function or interrupt content.
 */
int32_t add_timer(struct hv_timer *timer);

/**
 * @brief Delete a timer.
 *
 * @param[in] timer Pointer to timer.
 *
 * @return None
 *
 * @remark Don't call it in the timer callback function or interrupt content.
 */
void del_timer(struct hv_timer *timer);

/**
 * @brief Initialize timer.
 *
 * @return None
 */
void timer_init(void);

/**
 * @brief Calibrate tsc.
 *
 * @return None
 */
void calibrate_tsc(void);

/**
 * @}
 */

#endif /* TIMER_H */
