/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TIMER_H
#define TIMER_H

typedef int (*timer_handle_t)(void *);

enum tick_mode {
	TICK_MODE_ONESHOT = 0,
	TICK_MODE_PERIODIC,
};


struct timer {
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
static inline void initialize_timer(struct timer *timer,
				timer_handle_t func,
				void *priv_data,
				uint64_t fire_tsc,
				int mode,
				uint64_t period_in_cycle)
{
	if (timer) {
		timer->func = func;
		timer->priv_data = priv_data;
		timer->fire_tsc = fire_tsc;
		timer->mode = mode;
		timer->period_in_cycle = period_in_cycle;
		INIT_LIST_HEAD(&timer->node);
	}
}

/*
 * Don't call add_timer/del_timer in the timer callback function.
 */
int add_timer(struct timer *timer);
void del_timer(struct timer *timer);

int timer_softirq(int pcpu_id);
void timer_init(void);
void timer_cleanup(void);
void check_tsc(void);
void calibrate_tsc(void);

#endif /* TIMER_H */
