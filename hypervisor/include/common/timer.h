/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef COMMON_TIMER_H
#define COMMON_TIMER_H

#include <list.h>
#include <ticks.h>

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
	uint64_t timeout;		/**< tsc deadline to interrupt */
	uint64_t period_in_cycle;	/**< period of the periodic timer in CPU ticks */
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
 * @param[in] timeout tsc deadline to interrupt.
 * @param[in] period_in_cycle period of the periodic timer in unit of TSC cycles.
 *
 * @remark Don't initialize a timer twice if it has been added to the timer list
 *         after calling add_timer. If you want to, delete the timer from the list first.
 */
void initialize_timer(struct hv_timer *timer,
		      timer_handle_t func, void *priv_data,
		      uint64_t timeout, uint64_t period_in_cycle);

/**
 * @brief Check a timer whether expired.
 *
 * @param[in] timer Pointer to timer.
 * @param[in] now to compare.
 * @param[in] delta Pointer to return the delta (timeout - now) if timer is not expired.
 *
 * @retval true if the timer is expired, false otherwise.
 */
bool timer_expired(const struct hv_timer *timer, uint64_t now, uint64_t *delta);

/**
 * @brief Check if a timer is active (in the timer list) or not.
 *
 * @param[in] timer Pointer to timer.
 *
 * @retval true if the timer is in timer list, false otherwise.
 */
bool timer_is_started(const struct hv_timer *timer);

/**
 * @brief Update a timer.
 *
 * @param[in] timer Pointer to timer.
 * @param[in] timeout deadline to interrupt.
 * @param[in] period period of the periodic timer in unit of CPU ticks.
 */
void update_timer(struct hv_timer *timer, uint64_t timeout, uint64_t period);

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
 * @remark Don't call it in the timer callback function or interrupt content.
 */
void del_timer(struct hv_timer *timer);

/**
 * @brief Initialize timer.
 */
void timer_init(void);

/**
 * @brief Set timeout value to arch specific timer.
 *
 * @param[in] cnt Timeout value to set.
 */
void arch_set_timer_count(uint64_t cnt);

/**
 * @brief Initialize arch specific timer.
 */
void arch_init_timer(void);

/**
 * @}
 */

#endif /* COMMON_TIMER_H */
