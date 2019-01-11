/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <sys/param.h>

struct acrn_timer {
	int32_t fd;
	int32_t clockid;
	struct mevent *mevp;
	void (*callback)(void *, uint64_t);
	void *callback_param;
};

int32_t
acrn_timer_init(struct acrn_timer *timer, void (*cb)(void *, uint64_t), void *param);
void
acrn_timer_deinit(struct acrn_timer *timer);
int32_t
acrn_timer_settime(struct acrn_timer *timer, const struct itimerspec *new_value);
int32_t
acrn_timer_settime_abs(struct acrn_timer *timer,
		const struct itimerspec *new_value);
int32_t
acrn_timer_gettime(struct acrn_timer *timer, struct itimerspec *cur_value);

#define NS_PER_SEC	(1000000000ULL)

static inline uint64_t
ts_to_ticks(const uint32_t freq, const struct timespec *const ts)
{
	uint64_t tv_sec_ticks, tv_nsec_ticks;

	tv_sec_ticks = ts->tv_sec * freq;
	tv_nsec_ticks = (ts->tv_nsec * freq) / NS_PER_SEC;

	return tv_sec_ticks + tv_nsec_ticks;
}

static inline void
ticks_to_ts(const uint32_t freq, const uint64_t ticks,
		struct timespec *const ts)
{
	uint64_t ns;

	ns = howmany(ticks * NS_PER_SEC, freq);

	ts->tv_sec = ns / NS_PER_SEC;
	ts->tv_nsec = ns % NS_PER_SEC;
}
#endif /* _VTIMER_ */
