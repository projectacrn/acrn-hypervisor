/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VRTC_H_
#define VRTC_H_

#define RTC_SECONDS	0x00	/* seconds */
#define RTC_SECALRM	0x01	/* seconds alarm */
#define RTC_MINUTES	0x02	/* minutes */
#define RTC_MINALRM	0x03	/* minutes alarm */
#define RTC_HOURS	0x04	/* hours */
#define RTC_HRSALRM	0x05	/* hours alarm */
#define RTC_WDAY	0x06	/* week day */
#define RTC_DAY		0x07	/* day of month */
#define RTC_MONTH	0x08	/* month of year */
#define RTC_YEAR	0x09	/* month of year */
#define RTC_STATUSA	0x0a	/* status register A */
#define	RTCSA_TUP	0x80	/* time update, don't look now */
#define	RTC_STATUSB	0x0b	/* status register B */
#define	RTCSB_24HR	0x02	/* 0 = 12 hours, 1 = 24	hours */
#define	RTCSB_BCD	0x04	/* 0 = BCD, 1 =	Binary coded time */
#define RTC_STATUSC	0x0c	/* status register C */
#define	RTC_STATUSD	0x0d	/* status register D (R) Lost Power */
#define	RTCSD_PWR	0x80	/* clock power OK */
#define	RTC_CENTURY	0x32	/* current century */

#define RTC_HOUR_PM	0x80

#define CMOS_ADDR_PORT     0x70
#define CMOS_DATA_PORT     0x71
#define bin2bcd(x)    ((((x) / 10) << 4) + (x) % 10)
#define bcd2bin(x)    (((x) & 0x0f) + ((((x) >> 4) & 0xf) * 10))

#ifdef DEBUG_RTC
# define RTC_DEBUG(format, ...)	printf(format, ## __VA_ARGS__)
#else
# define RTC_DEBUG(format, ...)	do { } while (0)
#endif

/* Some handy constants. */
#define SECDAY	(24 * 60 * 60)
#define SECYR	(SECDAY * 365)

/* Traditional POSIX base year */
#define	POSIX_BASE_YEAR	1970
#define	RTCSB_BIN	0x04
/*
 * Generic routines to convert between a POSIX date
 * (seconds since 1/1/1970) and yr/mo/day/hr/min/sec
 * Derived from NetBSD arch/hp300/hp300/clock.c
 */
#define	FEBRUARY	2
#define	days_in_year(y)	(leapyear(y) ? 366 : 365)
#define	days_in_month(y, m) \
	(month_days[(m) - 1] + (m == FEBRUARY ? leapyear(y) : 0))
/* Day of week. Days are counted from 1/1/1970, which was a Thursday */
#define	day_of_week(days)  (((days) + 4) % 7)


struct clktime {
	int	year;	/* year (4 digit year) */
	int	mon;	/* month (1 - 12) */
	int	day;	/* day (1 - 31) */
	int	hour;	/* hour (0 - 23) */
	int	min;	/* minute (0 - 59) */
	int	sec;	/* second (0 - 59) */
	int	dow;	/* day of week (0 - 6; 0 = Sunday) */
	int	century; /* century */
};

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
	uint8_t	nvram[36];
	uint8_t	century;
	uint8_t	nvram2[128 - 51];
} __attribute__((packed));

struct vrtc {
	struct vm *vm;
	spinlock_t mtx;
	uint32_t addr; /* RTC register to read or write */
	long delta;
	struct rtcdev rtcdev;
};

uint8_t convert_to_24hour(uint8_t bin_hour);
void cmos_get_ct_time(struct clktime *time);
void *vrtc_init(struct vm *vm);
void vrtc_deinit(struct vm *vm);

#endif /* VRTC_H_ */
