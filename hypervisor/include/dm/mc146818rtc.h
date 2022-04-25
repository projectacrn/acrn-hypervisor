/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Copyright (C) 2022 Intel Corporation.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)rtc.h	7.1 (Berkeley) 5/12/91
 * $FreeBSD$
 */

#ifndef _MC146818_RTC_H_
#define _MC146818_RTC_H_

/*
 * MC146818 RTC Register locations
 */

#define RTC_SEC	0x00	/* seconds */
#define RTC_SECALRM	0x01	/* seconds alarm */
#define RTC_MIN	0x02	/* minutes */
#define RTC_MINALRM	0x03	/* minutes alarm */
#define RTC_HRS	0x04	/* hours */
#define RTC_HRSALRM	0x05	/* hours alarm */
#define RTC_WDAY	0x06	/* week day */
#define RTC_DAY	0x07	/* day of month */
#define RTC_MONTH	0x08	/* month of year */
#define RTC_YEAR	0x09	/* year in century*/
#define RTC_CENTURY	0x32	/* century */

#define RTC_STATUSA	0x0a	/* status register A */
#define RTCSA_TUP	0x80U	/* time update, don't look now */

#define RTC_STATUSB	0x0b	/* status register B */
#define RTC_INTR	0x0c	/* status register C (R) interrupt source */
#define RTC_STATUSD	0x0d	/* status register D (R) Lost Power */

#endif /* _MC146818_RTC_H_ */
