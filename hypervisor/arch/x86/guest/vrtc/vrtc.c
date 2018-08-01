/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <vrtc.h>

static long rtc_to_secs(struct vrtc *vrtc);

static const uint8_t month_days[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* Convert 24-hour to 12-hour format
 * In: ct_hour=bin in 24-hour format
 * Return: bin in 12-hour format
 */
static int convert_to_12hour(uint8_t ct_hour)
{
	int ret_hour = ct_hour;

	/*
	 * clock representation:
	 *
	 *    12-hour format(1~12)	ct_hour (0~23)
	 *	12	AM		0 (midnight)
	 *	1 - 11	AM		1 - 11
	 *	12	PM		12 (noon)
	 *	1 - 11	PM		13 - 23
	 */
	switch (ct_hour) {
	case 0:
	case 12:
		ret_hour = 12;
		break;
	default:
		ret_hour = ct_hour % 12;
		break;
	}

	if (ct_hour >= 12)
		ret_hour |= RTC_HOUR_PM; /* set MSB to indicate PM */

	return ret_hour;
}

/* Convert 12-hour to 24-hour format
 * In: hour=bin in 12-hour format
 * Return: bin in 24-hour format (ct_hour)
 */
uint8_t convert_to_24hour(uint8_t hour)
{
	bool pm = hour & RTC_HOUR_PM;
	int ct_hour = hour & ~RTC_HOUR_PM;

	/*
	 * clock representation:
	 *
	 *    12-hour format(1~12)	ct_hour (0~23)
	 *	12	AM		0 (midnight)
	 *	1 - 11	AM		1 - 11
	 *	12	PM		12 (noon)
	 *	1 - 11	PM		13 - 23
	 */

	if (ct_hour == 12)
		ct_hour = 0;
	if (pm)
		ct_hour += 12;

	return ct_hour;
}

static int leapyear(int year)
{
	return  ((year % 4) == 0 && (year % 100) != 0) ||
		((year % 400) == 0);
}

static inline uint8_t bin_to_rtc(struct rtcdev *rtc, int val)
{
	return (rtc->reg_b & RTCSB_BIN) ? val : bin2bcd(val);
}

static int rtc_to_bin(struct rtcdev *rtc, int val)
{
	return (rtc->reg_b & RTCSB_BIN) ? val : bcd2bin(val);
}

/* The hour's AP/PM bit (0x80) needs special handling!
 * In: hour=bin or bcd in 12-hour or 24-hour format depending
 * on the rtc setting.
 * Return: bin in 24-hour format
 */
static int rtc_to_bin_hour(struct rtcdev *rtc, int hour)
{
	int ct_hour;

	ct_hour = rtc_to_bin(rtc, hour & ~RTC_HOUR_PM) | (hour & RTC_HOUR_PM);

	if (!(rtc->reg_b & RTCSB_24HR))
		ct_hour = convert_to_24hour(ct_hour);

	return ct_hour;
}

/* The hour's AP/PM bit (0x80) needs special handling!
 * In: hour=bin in 24-hour format
 * Return: bin or bcd in 12-hour or 24-hour format depending
 * on the rtc setting; if 12-hour format, set the PM bit if necessary.
 */
static inline uint8_t bin_to_rtc_hour(struct rtcdev *rtc, int ct_hour)
{
	int ret_hour;

	if (rtc->reg_b & RTCSB_24HR)
		ret_hour = ct_hour;
	else
		ret_hour = convert_to_12hour(ct_hour);

	ret_hour = bin_to_rtc(rtc, ret_hour & ~RTC_HOUR_PM) |
		(ret_hour & RTC_HOUR_PM);

	return ret_hour;
}

static long ct_to_secs(struct clktime *ct)
{
	int i, year, days;

	year = ct->year;

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

	return (((long)days * 24 + ct->hour) * 60 + ct->min) * 60
		+ ct->sec;
}

static long curtime_in_secs(void)
{
	struct clktime time;

	cmos_get_ct_time(&time);
	return ct_to_secs(&time);
}

static long vrtc_get_delta(struct vrtc *vrtc)
{
	return rtc_to_secs(vrtc) - curtime_in_secs();
}

static void secs_to_ct(long secs, struct clktime *ct)
{
	long i, year, days, rsec;

	days = secs / SECDAY;
	rsec = secs % SECDAY;

	ct->dow = day_of_week(days);

	/* Subtract out whole years, counting them in i. */
	for (year = POSIX_BASE_YEAR; days >= days_in_year(year); year++)
		days -= days_in_year(year);
	ct->year = year;

	/* Subtract out whole months, counting them in i. */
	for (i = 1; days >= (long)days_in_month(year, i); i++)
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
}

static void ct_to_rtc(struct clktime *ct, struct rtcdev *rtc)
{
	rtc->sec = bin_to_rtc(rtc, ct->sec);
	rtc->min = bin_to_rtc(rtc, ct->min);
	rtc->hour = bin_to_rtc_hour(rtc, ct->hour);

	rtc->day_of_week = bin_to_rtc(rtc, ct->dow + 1);
	rtc->day_of_month = bin_to_rtc(rtc, ct->day);
	rtc->month = bin_to_rtc(rtc, ct->mon);
	rtc->year = bin_to_rtc(rtc, ct->year % 100);
	rtc->century = bin_to_rtc(rtc, ct->year / 100);
}

static void secs_to_rtc(long secs, struct vrtc *vrtc)
{
	struct clktime ct;

	secs_to_ct(secs, &ct);
	ct_to_rtc(&ct, &vrtc->rtcdev);
}

static void rtc_to_ct(struct rtcdev *rtc, struct clktime *ct)
{
	int century, year;

	ct->sec = rtc_to_bin(rtc, rtc->sec);
	ct->min = rtc_to_bin(rtc, rtc->min);
	ct->hour = rtc_to_bin_hour(rtc, rtc->hour);

	ct->dow = -1;

	ct->day = rtc_to_bin(rtc, rtc->day_of_month);
	ct->mon = rtc_to_bin(rtc, rtc->month);
	year = rtc_to_bin(rtc, rtc->year);
	century = rtc_to_bin(rtc, rtc->century);
	ct->year = century * 100 + year;
}

static long rtc_to_secs(struct vrtc *vrtc)
{
	struct clktime ct;

	rtc_to_ct(&vrtc->rtcdev, &ct);
	return ct_to_secs(&ct);
}

static void vrtc_cur_rtcdev(struct vrtc *vrtc)
{
	long curtime;

	curtime = curtime_in_secs() + vrtc->delta;
	secs_to_rtc(curtime, vrtc);
}

static int validate_input(struct rtcdev *rtc, int offset, int value)
{
	if ((offset == RTC_HOURS) || (offset == RTC_HRSALRM))
		value = rtc_to_bin_hour(rtc, value);
	else
		value = rtc_to_bin(rtc, value);

	switch (offset) {
	case RTC_SECONDS:
	case RTC_SECALRM:
	case RTC_MINUTES:
	case RTC_MINALRM:
		if ((value < 0) || (value > 59))
			value = 0;
		break;
	case RTC_HOURS:
	case RTC_HRSALRM:
		if ((value < 0) || (value > 23))
			value = 0;
		break;
	case RTC_DAY:
		if ((value < 1) || (value > 31))
			value = 1;
		break;
	case RTC_MONTH:
		if ((value < 1) || (value > 12))
			value = 1;
		break;
	case RTC_YEAR:
		if ((value < 0) || (value > 99))
			value = 18;
		break;
	case RTC_CENTURY:
		if ((value < 0) || (value > 99))
			value = 20;
		break;
	default:
		break;
	}

	if ((offset == RTC_HOURS) || (offset == RTC_HRSALRM))
		return bin_to_rtc_hour(rtc, value);
	else
		return bin_to_rtc(rtc, value);
}

static uint32_t vrtc_read(__unused struct vm_io_handler *hdlr, struct vm *vm,
	uint16_t addr, __unused size_t width)
{
	struct vrtc *vrtc = vm->vrtc;
	uint8_t reg;
	struct rtcdev *rtc;
	uint32_t offset;

	rtc = &vrtc->rtcdev;
	offset = vrtc->addr;

	if (offset >= sizeof(struct rtcdev))
		return -1;

	if (addr == CMOS_ADDR_PORT)
		return vrtc->addr;

	spinlock_obtain(&vrtc->mtx);

	if (offset < 10 || offset == RTC_CENTURY)
		vrtc_cur_rtcdev(vrtc);

	reg = *((uint8_t *)rtc + offset);

	if (offset == RTC_STATUSA) {
		reg ^= RTCSA_TUP;
		rtc->reg_a = reg;
	}

	spinlock_release(&vrtc->mtx);

	return reg;
}

static void vrtc_write(__unused struct vm_io_handler *hdlr, struct vm *vm, uint16_t addr,
	size_t width, uint32_t value)
{
	struct vrtc *vrtc = vm->vrtc;
	struct rtcdev *rtc;
	uint32_t offset;

	rtc = &vrtc->rtcdev;

	if (width != 1)
		return;

	spinlock_obtain(&vrtc->mtx);
	if (addr == CMOS_ADDR_PORT) {
		vrtc->addr = value & 0x7f;
		spinlock_release(&vrtc->mtx);
		return;
	}

	offset = vrtc->addr;
	if (offset >= sizeof(struct rtcdev)) {
		spinlock_release(&vrtc->mtx);
		return;
	}

	if (offset < 10 || offset == RTC_CENTURY) {
		value = validate_input(rtc, offset, value);
		vrtc_cur_rtcdev(vrtc);
	}

	switch (offset) {
	case RTC_STATUSA ... RTC_STATUSD:
		*((uint8_t *)rtc + offset) = value;
		break;

	case RTC_SECONDS:
		/* High order bit of 'seconds' is read only.*/
		value &= 0x7f;
	default:
		*((uint8_t *)rtc + offset) = value;
		vrtc->delta = vrtc_get_delta(vrtc);
	}

	spinlock_release(&vrtc->mtx);
}

void *vrtc_init(struct vm *vm)
{
	struct vrtc *vrtc;
	struct rtcdev *rtc;
	long curtime;
	struct vm_io_range range = {
	.flags = IO_ATTR_RW, .base = CMOS_ADDR_PORT, .len = 2};

	vrtc = calloc(1, sizeof(struct vrtc));
	ASSERT(vrtc != NULL, "");

	vrtc->vm = vm;
	spinlock_init(&vrtc->mtx);

	spinlock_obtain(&vrtc->mtx);
	rtc = &vrtc->rtcdev;
	rtc->reg_a = 0x20;
	rtc->reg_b = RTCSB_24HR;
	rtc->reg_c = 0;
	rtc->reg_d = RTCSD_PWR;
	vrtc->addr = RTC_STATUSD;

	curtime = curtime_in_secs();
	vrtc->delta = 0;
	secs_to_rtc(curtime, vrtc);

	spinlock_release(&vrtc->mtx);

	register_io_emulation_handler(vm, &range, vrtc_read, vrtc_write);

	return vrtc;
}

void vrtc_deinit(struct vm *vm)
{
	if (vm->vrtc == NULL)
		return;

	free(vm->vrtc);
	vm->vrtc = NULL;
}
