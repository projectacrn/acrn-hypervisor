/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VRTC_H
#define VRTC_H

typedef int32_t time_t;

/* Register layout of the RTC */
struct rtcdev {
	uint8_t	sec;
	uint8_t	alarm_sec;
	uint8_t	min;
	uint8_t	alarm_min;
	uint8_t	hour;
	uint8_t	alarm_hour;
	uint8_t	day_of_week;
	uint8_t	day_of_month;
	uint8_t	month;
	uint8_t	year;
	uint8_t	reg_a;
	uint8_t	reg_b;
	uint8_t	reg_c;
	uint8_t	reg_d;
	uint8_t	res[36];
	uint8_t	century;
};

struct acrn_vrtc {
	struct acrn_vm	*vm;
	uint32_t	addr;           /* RTC register to read or write */

	time_t		base_rtctime;	/* Base time calulated from physical rtc register. */
	uint64_t	base_tsc;	/* Base tsc value */

	struct rtcdev	rtcdev;		/* RTC register */
};

#endif /* VRTC_H */
