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

static spinlock_t cmos_lock = { .head = 0, .tail = 0 };

static uint8_t cmos_read(uint32_t addr)
{
	io_write_byte(addr, CMOS_ADDR_PORT);
	return io_read_byte(CMOS_DATA_PORT);
}

static bool cmos_update_in_progress(void)
{
	return (cmos_read(RTC_STATUSA) & RTCSA_TUP)?1:0;
}

void cmos_get_ct_time(struct clktime *time)
{
	uint8_t reg_b;
	int tries = 2000;

	spinlock_obtain(&cmos_lock);

	/* Make sure an update isn't in progress */
	while (cmos_update_in_progress() && tries--)
	;

	time->year = cmos_read(RTC_YEAR);
	time->mon = cmos_read(RTC_MONTH);
	time->day = cmos_read(RTC_DAY);
	time->hour = cmos_read(RTC_HOURS);
	time->min = cmos_read(RTC_MINUTES);
	time->sec = cmos_read(RTC_SECONDS);
	time->century = cmos_read(RTC_CENTURY);
	reg_b = cmos_read(RTC_STATUSB);

	spinlock_release(&cmos_lock);

	if (!(reg_b & RTCSB_BCD)) {
		time->sec = bcd2bin(time->sec);
		time->min = bcd2bin(time->min);

		time->hour = bcd2bin(time->hour & ~RTC_HOUR_PM)
			| (time->hour & RTC_HOUR_PM);

		time->day = bcd2bin(time->day);
		time->mon = bcd2bin(time->mon);
		time->year = bcd2bin(time->year);
		time->century = bcd2bin(time->century);
	}

	/* Convert 12 hour clock to 24 hour clock if necessary */
	if (!(reg_b & RTCSB_24HR))
		time->hour = convert_to_24hour(time->hour);

	time->year += time->century * 100;
}

