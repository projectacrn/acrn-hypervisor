/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _TIMER_H_
#define _TIMER_H_

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

#endif /* _VTIMER_ */
