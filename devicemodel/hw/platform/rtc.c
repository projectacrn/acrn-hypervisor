/*-
 * Copyright (c) 2014, Neel Natu (neel@freebsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "vmmapi.h"
#include "inout.h"
#include "mc146818rtc.h"
#include "rtc.h"
#include "mevent.h"
#include "timer.h"
#include "acpi.h"
#include "lpc.h"

/* #define DEBUG_RTC */
#ifdef DEBUG_RTC
# define RTC_DEBUG(format, ...)      printf(format, ## __VA_ARGS__)
#else
# define RTC_DEBUG(format, ...)      do { } while (0)
#endif

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
} __packed;

struct vrtc {
	struct vmctx *vm;
	pthread_mutex_t	mtx;
	struct acrn_timer update_timer;     /* timer for update interrupt */
	struct acrn_timer periodic_timer;   /* timer for periodic interrupt */
	u_int		addr;               /* RTC register to read or write */
	time_t		base_uptime;
	time_t		base_rtctime;
	struct rtcdev	rtcdev;
};

/*
 * Structure to hold the values typically reported by time-of-day clocks.
 * This can be passed to the generic conversion functions to be converted
 * to a struct timespec.
 */
struct clktime {
	int	year;			/* year (4 digit year) */
	int	mon;			/* month (1 - 12) */
	int	day;			/* day (1 - 31) */
	int	hour;			/* hour (0 - 23) */
	int	min;			/* minute (0 - 59) */
	int	sec;			/* second (0 - 59) */
	int	dow;			/* day of week (0 - 6; 0 = Sunday) */
	long nsec;			/* nano seconds */
};

/* Some handy constants. */
#define SECDAY		(24 * 60 * 60)
#define SECYR		(SECDAY * 365)

/* Traditional POSIX base year */
#define	POSIX_BASE_YEAR	1970

#define	SBT_1S	((time_t)1000000000LL)
#define	SBT_1M	(SBT_1S * 60)
#define	SBT_1MS	(SBT_1S / 1000)
#define	SBT_1US	(SBT_1S / 1000000)
#define	SBT_1NS	(SBT_1S / 1000000000)
#define	SBT_MAX	0x7fffffffffffffffLL

/*
 * RTC time is considered "broken" if:
 * - RTC updates are halted by the guest
 * - RTC date/time fields have invalid values
 */
#define	VRTC_BROKEN_TIME	((time_t)-1)

/* signo of timers created by vrtc */
#define	VRTC_TIMER_SIGNO	SIGALRM

#define	RTC_IRQ			8
#define	RTCSB_BIN		0x04
#define	RTCSB_ALL_INTRS		(RTCSB_UINTR | RTCSB_AINTR | RTCSB_PINTR)
#define	rtc_halted(rtc)		((rtc->rtcdev.reg_b & RTCSB_HALT) != 0)
#define	aintr_enabled(rtc)	(((rtc)->rtcdev.reg_b & RTCSB_AINTR) != 0)
#define	pintr_enabled(rtc)	(((rtc)->rtcdev.reg_b & RTCSB_PINTR) != 0)
#define	uintr_enabled(rtc)	(((rtc)->rtcdev.reg_b & RTCSB_UINTR) != 0)

/*--------------------------------------------------------------------*
 * Generic routines to convert between a POSIX date
 * (seconds since 1/1/1970) and yr/mo/day/hr/min/sec
 * Derived from NetBSD arch/hp300/hp300/clock.c
 */

#define	FEBRUARY	2
#define	days_in_year(y)	(leapyear(y) ? 366 : 365)
#define	days_in_month(y, m) \
	(month_days[(m) - 1] + (m == FEBRUARY ? leapyear(y) : 0))
/* Day of week. Days are counted from 1/1/1970, which was a Thursday */
#define	day_of_week(days)	(((days) + 4) % 7)

static const int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};


/*
 * This inline avoids some unnecessary modulo operations
 * as compared with the usual macro:
 *   ( ((year % 4) == 0 &&
 *      (year % 100) != 0) ||
 *     ((year % 400) == 0) )
 * It is otherwise equivalent.
 */
static int
leapyear(int year)
{
	int rv = 0;

	if ((year & 3) == 0) {
		rv = 1;
		if ((year % 100) == 0) {
			rv = 0;
			if ((year % 400) == 0)
				rv = 1;
		}
	}
	return rv;
}

u_char const bin2bcd_data[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99
};

static void vrtc_set_reg_c(struct vrtc *vrtc, uint8_t newval);

static int rtc_flag_broken_time = 1;

static inline int
divider_enabled(int reg_a)
{
	/*
	 * The RTC is counting only when dividers are not held in reset.
	 */
	return ((reg_a & 0x70) == 0x20);
}

static inline int
update_enabled(struct vrtc *vrtc)
{
	/*
	 * RTC date/time can be updated only if:
	 * - divider is not held in reset
	 * - guest has not disabled updates
	 * - the date/time fields have valid contents
	 */
	if (!divider_enabled(vrtc->rtcdev.reg_a))
		return false;

	if (rtc_halted(vrtc))
		return false;

	if (vrtc->base_rtctime == VRTC_BROKEN_TIME)
		return false;

	return true;
}

static time_t
vrtc_curtime(struct vrtc *vrtc, time_t *basetime)
{
	time_t now, delta;
	time_t t, secs;

	t = vrtc->base_rtctime;
	*basetime = vrtc->base_uptime;
	if (update_enabled(vrtc)) {
		now = time(NULL);
		delta = now - vrtc->base_uptime;
		assert(delta >= 0);

		secs = delta;
		t += secs;
		*basetime += secs;
	}
	return t;
}

static int
clk_ct_to_ts(struct clktime *ct, struct timespec *ts)
{
	int i, year, days;

	year = ct->year;

	/* Sanity checks. */
	if (ct->mon < 1 || ct->mon > 12 || ct->day < 1 ||
			ct->day > days_in_month(year, ct->mon) ||
			ct->hour > 23 ||  ct->min > 59 || ct->sec > 59 ||
			(sizeof(time_t) == 4 && year > 2037)) {
		/* time_t overflow */
		return -1;
	}

	/*
	 * Compute days since start of time
	 * First from years, then from months.
	 */
	days = 0;
	for (i = POSIX_BASE_YEAR; i < year; i++)
		days += days_in_year(i);

	/* Months */
	for (i = 1; i < ct->mon; i++)
		days += days_in_month(year, i);
	days += (ct->day - 1);

	ts->tv_sec = (((time_t)days * 24 + ct->hour) * 60 + ct->min) * 60 +
		ct->sec;
	ts->tv_nsec = ct->nsec;
	return 0;
}

static void
clk_ts_to_ct(struct timespec *ts, struct clktime *ct)
{
	time_t i, year, days;
	time_t rsec;	/* remainder seconds */
	time_t secs;

	secs = ts->tv_sec;
	days = secs / SECDAY;
	rsec = secs % SECDAY;

	ct->dow = day_of_week(days);

	/* Subtract out whole years, counting them in i. */
	for (year = POSIX_BASE_YEAR; days >= days_in_year(year); year++)
		days -= days_in_year(year);
	ct->year = year;

	/* Subtract out whole months, counting them in i. */
	for (i = 1; days >= days_in_month(year, i); i++)
		days -= days_in_month(year, i);
	ct->mon = i;

	/* Days are what is left over (+1) from all that. */
	ct->day = days + 1;

	/* Hours, minutes, seconds are easy */
	ct->hour = rsec / 3600;
	rsec = rsec % 3600;
	ct->min  = rsec / 60;
	rsec = rsec % 60;
	ct->sec  = rsec;
	ct->nsec = ts->tv_nsec;

	assert(ct->year >= 0 && ct->year < 10000);
	assert(ct->mon >= 1 && ct->mon <= 12);
	assert(ct->day >= 1 && ct->day <= 31);
	assert(ct->hour >= 0 && ct->hour <= 23);
	assert(ct->min >= 0 && ct->min <= 59);
	/* Not sure if this interface needs to handle leapseconds or not. */
	assert(ct->sec >= 0 && ct->sec <= 60);
}

static inline uint8_t
rtcset(struct rtcdev *rtc, int val)
{
	assert(val >= 0 && val < 100);

	return ((rtc->reg_b & RTCSB_BIN) ? val : bin2bcd_data[val]);
}

static time_t
vrtc_freq(struct vrtc *vrtc)
{
	int ratesel;

	static time_t pf[16] = {
		0,
		SBT_1S / 256,
		SBT_1S / 128,
		SBT_1S / 8192,
		SBT_1S / 4096,
		SBT_1S / 2048,
		SBT_1S / 1024,
		SBT_1S / 512,
		SBT_1S / 256,
		SBT_1S / 128,
		SBT_1S / 64,
		SBT_1S / 32,
		SBT_1S / 16,
		SBT_1S / 8,
		SBT_1S / 4,
		SBT_1S / 2,
	};

	/*
	 * If both periodic and alarm interrupts are enabled then use the
	 * periodic frequency to drive the callout. The minimum periodic
	 * frequency (2 Hz) is higher than the alarm frequency (1 Hz) so
	 * piggyback the alarm on top of it. The same argument applies to
	 * the update interrupt.
	 */
	if (pintr_enabled(vrtc) && divider_enabled(vrtc->rtcdev.reg_a)) {
		ratesel = vrtc->rtcdev.reg_a & 0xf;
		return pf[ratesel];
	} else if (aintr_enabled(vrtc) && update_enabled(vrtc))
		return SBT_1S;
	else if (uintr_enabled(vrtc) && update_enabled(vrtc))
		return SBT_1S;
	else
		return 0;
}

static int
rtcget(struct rtcdev *rtc, int val, int *retval)
{
	uint8_t upper, lower;

	if (rtc->reg_b & RTCSB_BIN) {
		*retval = val;
		return 0;
	}

	lower = val & 0xf;
	upper = (val >> 4) & 0xf;

	if (lower > 9 || upper > 9)
		return -1;

	*retval = upper * 10 + lower;
	return 0;
}

static void
secs_to_rtc(time_t rtctime, struct vrtc *vrtc, int force_update)
{
	struct clktime ct;
	struct timespec ts;
	struct rtcdev *rtc;
	int hour;

	if (rtctime < 0) {
		assert(rtctime == VRTC_BROKEN_TIME);
		return;
	}

	/*
	 * If the RTC is halted then the guest has "ownership" of the
	 * date/time fields. Don't update the RTC date/time fields in
	 * this case (unless forced).
	 */
	if (rtc_halted(vrtc) && !force_update)
		return;

	ts.tv_sec = rtctime;
	ts.tv_nsec = 0;
	clk_ts_to_ct(&ts, &ct);

	assert(ct.sec >= 0 && ct.sec <= 59);
	assert(ct.min >= 0 && ct.min <= 59);
	assert(ct.hour >= 0 && ct.hour <= 23);
	assert(ct.dow >= 0 && ct.dow <= 6);
	assert(ct.day >= 1 && ct.day <= 31);
	assert(ct.mon >= 1 && ct.mon <= 12);
	assert(ct.year >= POSIX_BASE_YEAR);

	rtc = &vrtc->rtcdev;
	rtc->sec = rtcset(rtc, ct.sec);
	rtc->min = rtcset(rtc, ct.min);

	if (rtc->reg_b & RTCSB_24HR) {
		hour = ct.hour;
	} else {
		/*
		 * Convert to the 12-hour format.
		 */
		switch (ct.hour) {
		case 0:			/* 12 AM */
		case 12:		/* 12 PM */
			hour = 12;
			break;
		default:
			/*
			 * The remaining 'ct.hour' values are interpreted as:
			 * [1  - 11] ->  1 - 11 AM
			 * [13 - 23] ->  1 - 11 PM
			 */
			hour = ct.hour % 12;
			break;
		}
	}

	rtc->hour = rtcset(rtc, hour);

	if ((rtc->reg_b & RTCSB_24HR) == 0 && ct.hour >= 12)
		rtc->hour |= 0x80;	    /* set MSB to indicate PM */

	rtc->day_of_week = rtcset(rtc, ct.dow + 1);
	rtc->day_of_month = rtcset(rtc, ct.day);
	rtc->month = rtcset(rtc, ct.mon);
	rtc->year = rtcset(rtc, ct.year % 100);
	rtc->century = rtcset(rtc, ct.year / 100);
}

static time_t
rtc_to_secs(struct vrtc *vrtc)
{
	struct clktime ct;
	struct timespec ts;
	struct rtcdev *rtc;
	int century, error, hour, pm, year;

	rtc = &vrtc->rtcdev;
	bzero(&ct, sizeof(struct clktime));
	error = rtcget(rtc, rtc->sec, &ct.sec);
	if (error || ct.sec < 0 || ct.sec > 59) {
		RTC_DEBUG("Invalid RTC sec %#x/%d", rtc->sec, ct.sec);
		goto fail;
	}

	error = rtcget(rtc, rtc->min, &ct.min);
	if (error || ct.min < 0 || ct.min > 59) {
		RTC_DEBUG("Invalid RTC min %#x/%d", rtc->min, ct.min);
		goto fail;
	}

	pm = 0;
	hour = rtc->hour;
	if ((rtc->reg_b & RTCSB_24HR) == 0) {
		if (hour & 0x80) {
			hour &= ~0x80;
			pm = 1;
		}
	}
	error = rtcget(rtc, hour, &ct.hour);
	if ((rtc->reg_b & RTCSB_24HR) == 0) {
		if (ct.hour >= 1 && ct.hour <= 12) {
			/*
			 * Convert from 12-hour format to internal 24-hour
			 * representation as follows:
			 *
			 *    12-hour format		ct.hour
			 *	12	AM		0
			 *	1 - 11	AM		1 - 11
			 *	12	PM		12
			 *	1 - 11	PM		13 - 23
			 */
			if (ct.hour == 12)
				ct.hour = 0;
			if (pm)
				ct.hour += 12;
		} else {
			RTC_DEBUG("Invalid RTC 12-hour format %#x/%d",
					rtc->hour, ct.hour);
			goto fail;
		}
	}

	if (error || ct.hour < 0 || ct.hour > 23) {
		RTC_DEBUG("Invalid RTC hour %#x/%d", rtc->hour, ct.hour);
		goto fail;
	}

	/*
	 * Ignore 'rtc->dow' because some guests like Linux don't bother
	 * setting it at all while others like OpenBSD/i386 set it incorrectly.
	 *
	 * clock_ct_to_ts() does not depend on 'ct.dow' anyways so ignore it.
	 */
	ct.dow = -1;

	error = rtcget(rtc, rtc->day_of_month, &ct.day);
	if (error || ct.day < 1 || ct.day > 31) {
		RTC_DEBUG("Invalid RTC mday %#x/%d", rtc->day_of_month,
				ct.day);
		goto fail;
	}

	error = rtcget(rtc, rtc->month, &ct.mon);
	if (error || ct.mon < 1 || ct.mon > 12) {
		RTC_DEBUG("Invalid RTC month %#x/%d", rtc->month, ct.mon);
		goto fail;
	}

	error = rtcget(rtc, rtc->year, &year);
	if (error || year < 0 || year > 99) {
		RTC_DEBUG("Invalid RTC year %#x/%d", rtc->year, year);
		goto fail;
	}

	error = rtcget(rtc, rtc->century, &century);
	ct.year = century * 100 + year;
	if (error || ct.year < POSIX_BASE_YEAR) {
		RTC_DEBUG("Invalid RTC century %#x/%d", rtc->century,
				ct.year);
		goto fail;
	}

	error = clk_ct_to_ts(&ct, &ts);
	if (error || ts.tv_sec < 0) {
		RTC_DEBUG("Invalid RTC clocktime.date %04d-%02d-%02d",
				ct.year, ct.mon, ct.day);
		RTC_DEBUG("Invalid RTC clocktime.time %02d:%02d:%02d",
				ct.hour, ct.min, ct.sec);
		goto fail;
	}
	return ts.tv_sec;		/* success */
fail:
	/*
	 * Stop updating the RTC if the date/time fields programmed by
	 * the guest are invalid.
	 */
	RTC_DEBUG("Invalid RTC date/time programming detected");
	return VRTC_BROKEN_TIME;
}

static void
vrtc_start_timer(struct acrn_timer *timer, time_t sec, time_t nsec)
{
	struct itimerspec ts;

	/*setting the interval time*/
	ts.it_interval.tv_sec = sec;
	ts.it_interval.tv_nsec = nsec;
	/*set the delay time it will be started when timer_setting*/
	ts.it_value.tv_sec = sec;
	ts.it_value.tv_nsec = nsec;
	assert(acrn_timer_settime(timer, &ts) == 0);
}

static int
vrtc_time_update(struct vrtc *vrtc, time_t newtime, time_t newbase)
{
	struct rtcdev *rtc;
	time_t oldtime;
	uint8_t alarm_sec, alarm_min, alarm_hour;

	rtc = &vrtc->rtcdev;
	alarm_sec = rtc->alarm_sec;
	alarm_min = rtc->alarm_min;
	alarm_hour = rtc->alarm_hour;

	oldtime = vrtc->base_rtctime;
	RTC_DEBUG("Updating RTC secs from %#lx to %#lx\n",
			oldtime, newtime);

	RTC_DEBUG("Updating RTC base uptime from %#lx to %#lx\n",
			vrtc->base_uptime, newbase);
	vrtc->base_uptime = newbase;

	if (newtime == oldtime)
		return 0;

	/*
	 * If 'newtime' indicates that RTC updates are disabled then just
	 * record that and return. There is no need to do alarm interrupt
	 * processing in this case.
	 */
	if (newtime == VRTC_BROKEN_TIME) {
		vrtc->base_rtctime = VRTC_BROKEN_TIME;
		return 0;
	}

	/*
	 * Return an error if RTC updates are halted by the guest.
	 */
	if (rtc_halted(vrtc)) {
		RTC_DEBUG("RTC update halted by guest\n");
		return -1;
	}

	do {
		/*
		 * If the alarm interrupt is enabled and 'oldtime' is valid
		 * then visit all the seconds between 'oldtime' and 'newtime'
		 * to check for the alarm condition.
		 *
		 * Otherwise move the RTC time forward directly to 'newtime'.
		 */
		if (aintr_enabled(vrtc) && oldtime != VRTC_BROKEN_TIME)
			vrtc->base_rtctime++;
		else
			vrtc->base_rtctime = newtime;

		if (aintr_enabled(vrtc)) {
			/*
			 * Update the RTC date/time fields before checking
			 * if the alarm conditions are satisfied.
			 */
			secs_to_rtc(vrtc->base_rtctime, vrtc, 0);

			if ((alarm_sec >= 0xC0 || alarm_sec == rtc->sec) &&
				(alarm_min >= 0xC0 || alarm_min == rtc->min) &&
					(alarm_hour >= 0xC0 ||
					 alarm_hour == rtc->hour)) {
				vrtc_set_reg_c(vrtc, rtc->reg_c | RTCIR_ALARM);
			}
		}
	} while (vrtc->base_rtctime != newtime);

	if (uintr_enabled(vrtc))
		vrtc_set_reg_c(vrtc, rtc->reg_c | RTCIR_UPDATE);

	return 0;
}

static void
vrtc_periodic_timer(void *arg, uint64_t nexp)
{
	struct vrtc *vrtc = arg;

	pthread_mutex_lock(&vrtc->mtx);

	if (pintr_enabled(vrtc))
		vrtc_set_reg_c(vrtc, vrtc->rtcdev.reg_c | RTCIR_PERIOD);

	pthread_mutex_unlock(&vrtc->mtx);
}

static void
vrtc_update_timer(void *arg, uint64_t nexp)
{
	struct vrtc *vrtc = arg;
	time_t basetime;
	time_t curtime;

	pthread_mutex_lock(&vrtc->mtx);

	if (aintr_enabled(vrtc) || uintr_enabled(vrtc)) {
		curtime = vrtc_curtime(vrtc, &basetime);
		vrtc_time_update(vrtc, curtime, basetime);
	}

	pthread_mutex_unlock(&vrtc->mtx);
}

static void
vrtc_set_reg_c(struct vrtc *vrtc, uint8_t newval)
{
	struct rtcdev *rtc;
	int oldirqf, newirqf;
	uint8_t oldval, changed;

	rtc = &vrtc->rtcdev;
	newval &= RTCIR_ALARM | RTCIR_PERIOD | RTCIR_UPDATE;

	oldirqf = rtc->reg_c & RTCIR_INT;

	if ((aintr_enabled(vrtc) && (newval & RTCIR_ALARM) != 0) ||
			(pintr_enabled(vrtc) && (newval & RTCIR_PERIOD) != 0) ||
			(uintr_enabled(vrtc) && (newval & RTCIR_UPDATE) != 0)) {
		newirqf = RTCIR_INT;
	} else {
		newirqf = 0;
	}

	oldval = rtc->reg_c;
	rtc->reg_c = newirqf | newval;
	changed = oldval ^ rtc->reg_c;

	if (changed) {
		RTC_DEBUG("RTC reg_c changed from %#x to %#x\n",
				oldval, rtc->reg_c);
	}

	if (!oldirqf && newirqf) {
		vm_set_gsi_irq(vrtc->vm, RTC_IRQ, GSI_SET_HIGH);
		RTC_DEBUG("RTC irq %d asserted\n", RTC_IRQ);
	} else if (oldirqf && !newirqf) {
		vm_set_gsi_irq(vrtc->vm, RTC_IRQ, GSI_SET_LOW);
		RTC_DEBUG("RTC irq %d deasserted\n", RTC_IRQ);
	}
}

static int
vrtc_set_reg_b(struct vrtc *vrtc, uint8_t newval)
{
	struct rtcdev *rtc;
	time_t oldfreq, newfreq, basetime;
	time_t curtime, rtctime;
	int error;
	uint8_t oldval, changed;

	rtc = &vrtc->rtcdev;
	oldval = rtc->reg_b;
	oldfreq = vrtc_freq(vrtc);

	rtc->reg_b = newval;
	changed = oldval ^ newval;
	if (changed) {
		RTC_DEBUG("RTC reg_b changed from %#x to %#x\n",
				oldval, newval);
	}

	if (changed & RTCSB_HALT) {
		if ((newval & RTCSB_HALT) == 0) {
			rtctime = rtc_to_secs(vrtc);
			basetime = time(NULL);
			if (rtctime == VRTC_BROKEN_TIME) {
				if (rtc_flag_broken_time)
					return -1;
			}
		} else {
			curtime = vrtc_curtime(vrtc, &basetime);
			assert(curtime == vrtc->base_rtctime);

			/*
			 * Force a refresh of the RTC date/time fields so
			 * they reflect the time right before the guest set
			 * the HALT bit.
			 */
			secs_to_rtc(curtime, vrtc, 1);

			/*
			 * Updates are halted so mark 'base_rtctime' to denote
			 * that the RTC date/time is in flux.
			 */
			rtctime = VRTC_BROKEN_TIME;
			rtc->reg_b &= ~RTCSB_UINTR;
		}
		error = vrtc_time_update(vrtc, rtctime, basetime);
		assert(error == 0);
	}

	/*
	 * Side effect of changes to the interrupt enable bits.
	 */
	if (changed & RTCSB_ALL_INTRS)
		vrtc_set_reg_c(vrtc, vrtc->rtcdev.reg_c);

	/*
	 * Change the callout frequency if it has changed.
	 */
	newfreq = vrtc_freq(vrtc);

	if (pintr_enabled(vrtc) && newfreq != oldfreq) {
		/*start the new periodic timer*/
		vrtc_start_timer(&vrtc->periodic_timer, 0, newfreq);

	} else {
		/*Nothing to do*/
	}

	/*
	 * The side effect of bits that control the RTC date/time format
	 * is handled lazily when those fields are actually read.
	 */
	return 0;
}

static void
vrtc_set_reg_a(struct vrtc *vrtc, uint8_t newval)
{
	time_t oldfreq, newfreq;
	uint8_t oldval, changed;

	newval &= ~RTCSA_TUP;
	oldval = vrtc->rtcdev.reg_a;
	oldfreq = vrtc_freq(vrtc);

	if (divider_enabled(oldval) && !divider_enabled(newval)) {
		RTC_DEBUG("RTC divider held in reset at %#lx/%#lx",
				vrtc->base_rtctime, vrtc->base_uptime);
	} else if (!divider_enabled(oldval) && divider_enabled(newval)) {
		/*
		 * If the dividers are coming out of reset then update
		 * 'base_uptime' before this happens. This is done to
		 * maintain the illusion that the RTC date/time was frozen
		 * while the dividers were disabled.
		 */
		vrtc->base_uptime = time(NULL);
		RTC_DEBUG("RTC divider out of reset at %#lx/%#lx",
				vrtc->base_rtctime, vrtc->base_uptime);
	} else {
		/* NOTHING */
	}

	vrtc->rtcdev.reg_a = newval;
	changed = oldval ^ newval;
	if (changed) {
		RTC_DEBUG("RTC reg_a changed from %#x to %#x",
				oldval, newval);
	}

	/*
	 * Side effect of changes to rate select and divider enable bits.
	 */
	newfreq = vrtc_freq(vrtc);

	if (pintr_enabled(vrtc) && newfreq != oldfreq) {
		/*start the new periodic timer*/
		vrtc_start_timer(&vrtc->periodic_timer, 0, newfreq);
	} else {
		/*Nothing to do*/
	}
}

int
vrtc_nvram_write(struct vrtc *vrtc, int offset, uint8_t value)
{
	uint8_t *ptr;

	/*
	 * Don't allow writes to RTC control registers or the date/time fields.
	 */
	if (offset < offsetof(struct rtcdev, nvram[0]) ||
			offset == RTC_CENTURY ||
			offset >= sizeof(struct rtcdev)) {
		RTC_DEBUG("RTC nvram write to invalid offset %d", offset);
		return -1;
	}

	pthread_mutex_lock(&vrtc->mtx);
	ptr = (uint8_t *)(&vrtc->rtcdev);
	ptr[offset] = value;
	RTC_DEBUG("RTC nvram write %#x to offset %#x", value, offset);
	pthread_mutex_unlock(&vrtc->mtx);

	return 0;
}

int
vrtc_addr_handler(struct vmctx *ctx, int vcpu, int in, int port,
		  int bytes, uint32_t *eax, void *arg)
{
	struct vrtc *vrtc = arg;

	if (bytes != 1)
		return -1;

	if (in) {
		*eax = 0xff;
		return 0;
	}

	pthread_mutex_lock(&vrtc->mtx);
	vrtc->addr = *eax & 0x7f;
	pthread_mutex_unlock(&vrtc->mtx);

	return 0;
}

int
vrtc_data_handler(struct vmctx *ctx, int vcpu, int in, int port,
		  int bytes, uint32_t *eax, void *arg)
{
	struct vrtc *vrtc = arg;
	struct rtcdev *rtc;
	time_t basetime;
	time_t curtime;
	int error, offset;

	rtc = &vrtc->rtcdev;

	if (bytes != 1)
		return -1;

	pthread_mutex_lock(&vrtc->mtx);
	offset = vrtc->addr;
	if (offset >= sizeof(struct rtcdev)) {
		pthread_mutex_unlock(&vrtc->mtx);
		return -1;
	}

	error = 0;
	curtime = vrtc_curtime(vrtc, &basetime);
	vrtc_time_update(vrtc, curtime, basetime);

	/*
	 * Update RTC date/time fields if necessary.
	 *
	 * This is not just for reads of the RTC. The side-effect of writing
	 * the century byte requires other RTC date/time fields (e.g. sec)
	 * to be updated here.
	 */
	if (offset < 10 || offset == RTC_CENTURY)
		secs_to_rtc(curtime, vrtc, 0);

	if (in) {
		if (offset == 12) {
			/*
			 * XXX
			 * reg_c interrupt flags are updated only if the
			 * corresponding interrupt enable bit in reg_b is set.
			 */
			*eax = vrtc->rtcdev.reg_c;
			vrtc_set_reg_c(vrtc, 0);
		} else {
			*eax = *((uint8_t *)rtc + offset);
		}
		RTC_DEBUG("Read value %#x from RTC offset %#x\n", *eax, offset);
	} else {
		switch (offset) {
		case 10:
			RTC_DEBUG("RTC reg_a set to %#x\n", *eax);
			vrtc_set_reg_a(vrtc, *eax);
			break;
		case 11:
			RTC_DEBUG("RTC reg_b set to %#x\n", *eax);
			error = vrtc_set_reg_b(vrtc, *eax);
			break;
		case 12:
			RTC_DEBUG("RTC reg_c set to %#x (ignored)\n", *eax);
			break;
		case 13:
			RTC_DEBUG("RTC reg_d set to %#x (ignored)\n", *eax);
			break;
		case 0:
			/*
			 * High order bit of 'seconds' is readonly.
			 */
			*eax &= 0x7f;
			/* FALLTHRU */
		default:
			RTC_DEBUG("RTC offset %#x set to %#x\n", offset, *eax);
			*((uint8_t *)rtc + offset) = *eax;
			break;
		}

		/*
		 * XXX some guests (e.g. OpenBSD) write the century byte
		 * outside of RTCSB_HALT so re-calculate the RTC date/time.
		 */
		if (offset == RTC_CENTURY && !rtc_halted(vrtc)) {
			curtime = rtc_to_secs(vrtc);
			error = vrtc_time_update(vrtc, curtime, time(NULL));
			assert(!error);
			if (curtime == VRTC_BROKEN_TIME && rtc_flag_broken_time)
				error = -1;
		}
	}

	pthread_mutex_unlock(&vrtc->mtx);

	return error;
}

int
vrtc_set_time(struct vrtc *vrtc, time_t secs)
{
	int error;

	pthread_mutex_lock(&vrtc->mtx);
	error = vrtc_time_update(vrtc, secs, time(NULL));
	pthread_mutex_unlock(&vrtc->mtx);

	if (error)
		RTC_DEBUG("Error %d setting RTC time to %#lx", error, secs);
	else
		RTC_DEBUG("RTC time set to %#lx", secs);

	return error;
}

void
vrtc_reset(struct vrtc *vrtc)
{
	struct rtcdev *rtc;

	pthread_mutex_lock(&vrtc->mtx);

	rtc = &vrtc->rtcdev;
	vrtc_set_reg_b(vrtc, rtc->reg_b & ~(RTCSB_ALL_INTRS | RTCSB_SQWE));
	vrtc_set_reg_c(vrtc, 0);

	pthread_mutex_unlock(&vrtc->mtx);
}

int
vrtc_init(struct vmctx *ctx)
{
	struct vrtc *vrtc;
	size_t lomem, himem;
	int err;
	struct rtcdev *rtc;
	time_t curtime;
	struct inout_port rtc_addr, rtc_data;

	vrtc = calloc(1, sizeof(struct vrtc));
	assert(vrtc != NULL);
	vrtc->vm = ctx;
	ctx->vrtc = vrtc;

	pthread_mutex_init(&vrtc->mtx, NULL);

	/*
	 * Report guest memory size in nvram cells as required by UEFI.
	 * Little-endian encoding.
	 * 0x34/0x35 - 64KB chunks above 16MB, below 4GB
	 * 0x5b/0x5c/0x5d - 64KB chunks above 4GB
	 */
	lomem = vm_get_lowmem_size(ctx);
	assert(lomem >= 16 * MB);
	lomem = (lomem - 16 * MB) / (64 * KB);
	err = vrtc_nvram_write(vrtc, RTC_LMEM_LSB, lomem);
	assert(err == 0);
	err = vrtc_nvram_write(vrtc, RTC_LMEM_MSB, lomem >> 8);
	assert(err == 0);

	himem = vm_get_highmem_size(ctx) / (64 * KB);
	err = vrtc_nvram_write(vrtc, RTC_HMEM_LSB, himem);
	assert(err == 0);
	err = vrtc_nvram_write(vrtc, RTC_HMEM_SB, himem >> 8);
	assert(err == 0);
	err = vrtc_nvram_write(vrtc, RTC_HMEM_MSB, himem >> 16);
	assert(err == 0);

	memset(&rtc_addr, 0, sizeof(struct inout_port));
	memset(&rtc_data, 0, sizeof(struct inout_port));
	/*register io port handler for rtc addr*/
	rtc_addr.name = "rtc";
	rtc_addr.port = IO_RTC;
	rtc_addr.size = 1;
	rtc_addr.flags = IOPORT_F_INOUT;
	rtc_addr.handler = vrtc_addr_handler;
	rtc_addr.arg = vrtc;
	assert(register_inout(&rtc_addr) == 0);

	/*register io port handler for rtc data*/
	rtc_data.name = "rtc";
	rtc_data.port = IO_RTC + 1;
	rtc_data.size = 1;
	rtc_data.flags = IOPORT_F_INOUT;
	rtc_data.handler = vrtc_data_handler;
	rtc_data.arg = vrtc;
	assert(register_inout(&rtc_data) == 0);

	/* Allow dividers o keep time but disable everything else */
	rtc = &vrtc->rtcdev;
	rtc->reg_a = 0x20;
	rtc->reg_b = RTCSB_24HR;
	rtc->reg_c = 0;
	rtc->reg_d = RTCSD_PWR;

	/* Reset the index register to a safe value. */
	vrtc->addr = RTC_STATUSD;

	/*
	 * Initialize RTC time to 00:00:00 Jan 1, 1970 if curtime = 0
	 */
	/*curtime = 0;*/
	curtime = time(NULL);

	pthread_mutex_lock(&vrtc->mtx);
	vrtc->base_rtctime = VRTC_BROKEN_TIME;
	vrtc_time_update(vrtc, curtime, time(NULL));
	secs_to_rtc(curtime, vrtc, 0);
	pthread_mutex_unlock(&vrtc->mtx);

	/* init periodic interrupt timer */
	vrtc->periodic_timer.clockid = CLOCK_REALTIME;
	acrn_timer_init(&vrtc->periodic_timer, vrtc_periodic_timer, vrtc);

	/* init update interrupt timer(1s)*/
	vrtc->update_timer.clockid = CLOCK_REALTIME;
	acrn_timer_init(&vrtc->update_timer, vrtc_update_timer, vrtc);
	vrtc_start_timer(&vrtc->update_timer, 1, 0);

	return 0;
}

void
vrtc_deinit(struct vmctx *ctx)
{
	struct vrtc *vrtc = ctx->vrtc;
	struct inout_port iop;

	/*deinit acrn_timer*/
	acrn_timer_deinit(&vrtc->periodic_timer);
	acrn_timer_deinit(&vrtc->update_timer);

	memset(&iop, 0, sizeof(struct inout_port));
	iop.name = "rtc";
	iop.port = IO_RTC;
	iop.size = 1;
	unregister_inout(&iop);

	memset(&iop, 0, sizeof(struct inout_port));
	iop.name = "rtc";
	iop.port = IO_RTC + 1;
	iop.size = 1;
	unregister_inout(&iop);

	free(vrtc);
	ctx->vrtc = NULL;
}

static void
rtc_dsdt(void)
{
	dsdt_line("");
	dsdt_line("Device (RTC)");
	dsdt_line("{");
	dsdt_line("  Name (_HID, EisaId (\"PNP0B00\"))");
	dsdt_line("  Name (_CRS, ResourceTemplate ()");
	dsdt_line("  {");
	dsdt_indent(2);
	dsdt_fixed_ioport(IO_RTC, 2);
	dsdt_fixed_irq(8);
	dsdt_unindent(2);
	dsdt_line("  })");
	dsdt_line("}");
}
LPC_DSDT(rtc_dsdt);

/*
 * Reserve the extended RTC I/O ports although they are not emulated at this
 * time.
 */
SYSRES_IO(0x72, 6);
