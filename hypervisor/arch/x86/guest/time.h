/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2018 Intel Corporation. All rights reserved.
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
 *	@(#)time.h	8.5 (Berkeley) 5/4/95
 * $FreeBSD$
 */

#ifndef _TIME_H_
#define	_TIME_H_

struct callout {
	void    *c_arg;                         /* function argument */
	void    (*c_func)(void *);              /* function to call */
	short   c_flags;                        /* User State */
};

#define CALLOUT_ACTIVE          0x0002 /* callout is currently active */
#define CALLOUT_PENDING         0x0004 /* callout is waiting for timeout */
#define callout_active(c)       ((c)->c_flags & CALLOUT_ACTIVE)
#define callout_deactivate(c)   ((c)->c_flags &= ~CALLOUT_ACTIVE)
#define callout_pending(c)      ((c)->c_flags & CALLOUT_PENDING)

typedef int64_t time_t;
typedef int64_t sbintime_t;

struct bintime {
	time_t	sec;
	uint64_t frac;
};

static inline void
bintime_add(struct bintime *_bt, const struct bintime *_bt2)
{
	uint64_t _u;

	_u = _bt->frac;
	_bt->frac += _bt2->frac;
	if (_u > _bt->frac)
		_bt->sec++;
	_bt->sec += _bt2->sec;
}

static inline void
bintime_sub(struct bintime *_bt, const struct bintime *_bt2)
{
	uint64_t _u;

	_u = _bt->frac;
	_bt->frac -= _bt2->frac;
	if (_u < _bt->frac)
		_bt->sec--;
	_bt->sec -= _bt2->sec;
}

static inline void
bintime_mul(struct bintime *_bt, uint32_t _x)
{
	uint64_t _p1, _p2;

	_p1 = (_bt->frac & 0xffffffffull) * _x;
	_p2 = (_bt->frac >> 32) * _x + (_p1 >> 32);
	_bt->sec *= _x;
	_bt->sec += (_p2 >> 32);
	_bt->frac = (_p2 << 32) | (_p1 & 0xffffffffull);
}

#define	bintime_cmp(a, b, cmp)						\
	(((a)->sec == (b)->sec) ?					\
	    ((a)->frac cmp(b)->frac) :					\
	    ((a)->sec cmp(b)->sec))

#define SBT_1S  ((sbintime_t)1 << 32)
#define SBT_1US (SBT_1S / 1000000)

#define BT2FREQ(bt)                                                     \
	(((uint64_t)0x8000000000000000 + ((bt)->frac >> 2)) /           \
	 ((bt)->frac >> 1))

#define FREQ2BT(freq, bt)                                               \
{                                                                       \
	(bt)->sec = 0;                                                  \
	(bt)->frac = ((uint64_t)0x8000000000000000  / (freq)) << 1;     \
}

static inline sbintime_t
bttosbt(const struct bintime _bt)
{

	return (((sbintime_t)_bt.sec << 32) + (_bt.frac >> 32));
}

#endif /* !_TIME_H_ */
