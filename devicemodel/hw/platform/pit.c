/*-
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2014 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <sysexits.h>
#include <string.h>

#include "vmmapi.h"
#include "timer.h"
#include "inout.h"
#include "pit.h"

#define	TMR2_OUT_STS		0x20

#define	PIT_8254_FREQ		(1193182)
#define	PIT_HZ_TO_TICKS(hz)	((PIT_8254_FREQ + (hz) / 2) / (hz))

#define PERIODIC_MODE(mode)	\
	((mode) == TIMER_RATEGEN || (mode) == TIMER_SQWAVE)

#define	VPIT_LOCK()													\
	do {															\
		int err;													\
		err = pthread_mutex_lock(&vpit_mtx);						\
		if (err)													\
			errx(EX_SOFTWARE, "pthread_mutex_lock returned %s",		\
					strerror(err));									\
	} while (0)

#define	VPIT_UNLOCK()												\
	do {															\
		int err;													\
		err = pthread_mutex_unlock(&vpit_mtx);						\
		if (err)													\
			errx(EX_SOFTWARE, "pthread_mutex_unlock returned %s",	\
					strerror(err));									\
	} while (0)

#define vpit_ts_to_ticks(ts)	ts_to_ticks(PIT_8254_FREQ, ts)
/* won't overflow since the max value of ticks is 65536 */
#define vpit_ticks_to_ts(tk, ts)	ticks_to_ts(PIT_8254_FREQ, tk, ts)


struct vpit_timer_arg {
	struct vpit	*vpit;
	int		channel_num;
	bool	active;
} vpit_timer_arg[3];

struct channel {
	int		mode;
	uint32_t	initial;	/* initial counter value */
	struct timespec start_ts;	/* uptime when counter was loaded */
	uint8_t		cr[2];
	uint8_t		ol[2];
	bool		nullcnt;
	bool		slatched;	/* status latched */
	uint8_t		status;
	int		crbyte;
	int		olbyte;
	int		frbyte;
	int		timer_idx;	/* only used by counter 0 */
	timer_t	timer_id;	/* only used by counter 0 */
};

struct vpit {
	struct vmctx	*vm;
	struct channel	channel[3];
};


/* one vPIT per VM */
static pthread_mutex_t vpit_mtx = PTHREAD_MUTEX_INITIALIZER;


static uint64_t
ticks_elapsed_since(const struct timespec *since)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts))
		errx(EX_SOFTWARE, "clock_gettime returned: %s", strerror(errno));

	if (timespeccmp(&ts, since, <=))
		return 0;

	timespecsub(&ts, since);
	return vpit_ts_to_ticks(&ts);
}

static bool
pit_cntr0_timer_running(struct vpit *vpit)
{
	struct channel *c;

	c = &vpit->channel[0];
	return vpit_timer_arg[c->timer_idx].active;
}

static int
vpit_get_out(struct vpit *vpit, int channel, uint64_t delta_ticks)
{
	struct channel *c;
	bool initval;
	int out = 1;

	c = &vpit->channel[channel];
	initval = c->nullcnt;

	/* XXX only channel 0 emulates delayed CE loading */
	if (channel == 0 && PERIODIC_MODE(c->mode)) {
		initval = initval && !pit_cntr0_timer_running(vpit);
	}

	switch (c->mode) {
	case TIMER_INTTC:
		/*
		 * For mode 0, see if the elapsed time is greater
		 * than the initial value - this results in the
		 * output pin being set to 1 in the status byte.
		 */
		out = (initval) ? 0 : (delta_ticks >= c->initial);
		break;
	case TIMER_RATEGEN:
		out = (initval) ?
			1 : (delta_ticks % c->initial != c->initial - 1);
		break;
	case TIMER_SQWAVE:
		out = (initval) ?
			1 : (delta_ticks % c->initial < (c->initial + 1) / 2);
		break;
	case TIMER_SWSTROBE:
		out = (initval) ? 1 : (delta_ticks != c->initial);
		break;
	default:
		errx(EX_SOFTWARE, "vpit invalid timer mode: %d", c->mode);
	}

	return out;
}

static uint32_t
pit_cr_val(uint8_t cr[2])
{
	uint32_t val;

	val = cr[0] | (uint16_t)cr[1] << 8;

	/* CR == 0 means 2^16 for binary counting */
	if (val == 0) {
		val = 0x10000;
	}

	return val;
}

static void
pit_load_ce(struct channel *c)
{
	/* no CR update in progress */
	if (c->nullcnt && c->crbyte == 2) {
		c->initial = pit_cr_val(c->cr);
		c->nullcnt = false;
		c->crbyte = 0;

		if (clock_gettime(CLOCK_REALTIME, &c->start_ts))
			errx(EX_SOFTWARE, "clock_gettime returned: %s", strerror(errno));

		if (c->initial == 0 || c->initial > 0x10000) {
			warnx("vpit invalid initial count: 0x%x - use 0x10000",
					c->initial);
			c->initial = 0x10000;
		}
	}
}

/*
 * Dangling timer callbacks must have a way to synchronize
 * with timer deletions and deinit.
 */
static void
vpit_timer_handler(union sigval s)
{
	struct vpit_timer_arg *arg;
	struct vpit *vpit;
	struct channel *c;

	arg = s.sival_ptr;

	VPIT_LOCK();

	/* skip if timer is no longer active */
	if (!arg->active) {
		goto done;
	}

	/* it's now safe to use the vpit pointer */
	vpit = arg->vpit;

	if (vpit == NULL) {
		warnx("vpit is NULL");
		goto done;
	}

	c = &vpit->channel[arg->channel_num];

	/* generate a rising edge on OUT */
	vm_set_gsi_irq(vpit->vm, PIT_IOAPIC_IRQ, GSI_RAISING_PULSE);

	/* CR -> CE if necessary */
	pit_load_ce(c);

done:
	VPIT_UNLOCK();
}

static bool
pit_timer_stop_cntr0(struct vpit *vpit, struct itimerspec *rem)
{
	struct channel *c;
	bool active;

	c = &vpit->channel[0];
	active = pit_cntr0_timer_running(vpit);

	if (active) {
		vpit_timer_arg[c->timer_idx].active = false;

		if (rem) {
			if (timer_gettime(c->timer_id, rem))
				errx(EX_SOFTWARE,
						"timer_gettime returned: %s", strerror(errno));
		}

		if (timer_delete(c->timer_id))
			errx(EX_SOFTWARE, "timer_delete returned: %s", strerror(errno));

		if (++c->timer_idx == nitems(vpit_timer_arg))
			c->timer_idx = 0;

		if (pit_cntr0_timer_running(vpit)) {
			warnx("vpit timer %d is still active", c->timer_idx);
			vpit_timer_arg[c->timer_idx].active = false;
		}
	}

	return active;
}

static void
pit_timer_start_cntr0(struct vpit *vpit)
{
	struct channel *c;
	struct itimerspec ts = { 0 };
	struct sigevent sigevt = { 0 };

	c = &vpit->channel[0];

	if (pit_timer_stop_cntr0(vpit, &ts) && PERIODIC_MODE(c->mode)) {
		/*
		 * Counter is being updated while counting in periodic mode.
		 * Update CE at the end of the current counting cycle.
		 *
		 * XXX
		 * pit_update_counter() or vpit_get_out() can be called
		 * right before vpit_timer_handler(), causing it to use a
		 * wrapped-around value that's inconsistent with the spec.
		 *
		 * On real hardware, mode 3 requires CE to be updated at the
		 * end of its current half-cycle. We operate as if CR is
		 * always updated in the second half-cycle (before a rising
		 * edge on OUT).
		 */
		if (!timespecisset(&ts.it_interval))
			warnx("vpit is in periodic mode but with a one-shot timer");

		/* ts.it_value contains the remaining time until expiration */
		vpit_ticks_to_ts(pit_cr_val(c->cr), &ts.it_interval);
	} else {
		/*
		 * Aperiodic mode or no running periodic counter.
		 * Update CE immediately.
		 */
		uint64_t timer_ticks;

		pit_load_ce(c);

		timer_ticks = (c->mode == TIMER_SWSTROBE) ?
		              c->initial + 1 : c->initial;
		vpit_ticks_to_ts(timer_ticks, &ts.it_value);

		/* make it periodic if required */
		if (PERIODIC_MODE(c->mode))
			ts.it_interval = ts.it_value;
		else if (timespecisset(&ts.it_interval)) {
			warnx("vpit is in aperiodic mode but with a periodic timer");
			memset(&ts.it_interval, 0, sizeof(ts.it_interval));
		}
	}

	sigevt.sigev_value.sival_ptr = &vpit_timer_arg[c->timer_idx];
	sigevt.sigev_notify = SIGEV_THREAD;
	sigevt.sigev_notify_function = vpit_timer_handler;

	if (timer_create(CLOCK_REALTIME, &sigevt, &c->timer_id))
		errx(EX_SOFTWARE, "timer_create returned: %s", strerror(errno));

	vpit_timer_arg[c->timer_idx].active = true;

	/* arm the timer */
	if (timer_settime(c->timer_id, 0, &ts, NULL))
		errx(EX_SOFTWARE, "timer_settime returned: %s", strerror(errno));
}

static uint16_t
pit_update_counter(struct vpit *vpit, struct channel *c, bool latch,
		uint64_t *ticks_elapsed)
{
	uint16_t lval = 0;
	uint64_t delta_ticks;

	if (c->initial == 0) {
		/*
		 * This is possibly an o/s bug - reading the value of
		 * the timer without having set up the initial value.
		 *
		 * The original Bhyve user-space version of this code
		 * set the timer to 100hz in this condition; do the same
		 * here.
		 */
		printf("vpit reading uninitialized counter value\n");

		c->initial = PIT_HZ_TO_TICKS(100);
		delta_ticks = 0;
		if (clock_gettime(CLOCK_REALTIME, &c->start_ts))
			errx(EX_SOFTWARE, "clock_gettime returned: %s", strerror(errno));
	} else
		delta_ticks = ticks_elapsed_since(&c->start_ts);

	switch (c->mode) {
	case TIMER_INTTC:
	case TIMER_SWSTROBE:
		lval = c->initial - delta_ticks;
		break;
	case TIMER_RATEGEN:
		lval = c->initial - delta_ticks % c->initial;
		break;
	case TIMER_SQWAVE: {
		uint64_t t = delta_ticks % c->initial;

		if (t >= (c->initial + 1) / 2)
			t -= (c->initial + 1) / 2;

		lval = (c->initial & ~0x1) - (t * 2);
		break;
	}
	default:
		errx(EX_SOFTWARE, "vpit invalid timer mode: %d", c->mode);
	}

	/* cannot latch a new value until the old one has been consumed */
	if (latch && c->olbyte == 0) {
		c->olbyte = 2;
		c->ol[1] = lval;		/* LSB */
		c->ol[0] = lval >> 8;		/* MSB */
	}

	*ticks_elapsed = delta_ticks;
	return lval;
}

static int
pit_readback1(struct vpit *vpit, int channel, uint8_t cmd)
{
	struct channel *c;
	uint64_t delta_ticks;

	c = &vpit->channel[channel];

	/*
	 * Latch the count/status of the timer if not already latched.
	 * N.B. that the count/status latch-select bits are active-low.
	 */
	pit_update_counter(vpit, c, !(cmd & TIMER_RB_LCTR), &delta_ticks);

	if (!(cmd & TIMER_RB_LSTATUS) && !c->slatched) {
		c->slatched = true;

		/* status byte is only updated upon latching */
		c->status = TIMER_16BIT | c->mode;

		if (c->nullcnt) {
			c->status |= TIMER_STS_NULLCNT;
		}

		/* use the same delta_ticks for both latches */
		if (vpit_get_out(vpit, channel, delta_ticks)) {
			c->status |= TIMER_STS_OUT;
		}
	}

	return (0);
}

static int
pit_readback(struct vpit *vpit, uint8_t cmd)
{
	int error = 0;

	/*
	 * The readback command can apply to all timers.
	 */
	if (cmd & TIMER_RB_CTR_0) {
		error = pit_readback1(vpit, 0, cmd);
	}
	if (!error && cmd & TIMER_RB_CTR_1) {
		error = pit_readback1(vpit, 1, cmd);
	}
	if (!error && cmd & TIMER_RB_CTR_2) {
		error = pit_readback1(vpit, 2, cmd);
	}

	return error;
}


static int
vpit_update_mode(struct vpit *vpit, uint8_t val)
{
	struct channel *c;
	int sel, rw, mode, channel;
	uint64_t delta_ticks;

	sel = val & TIMER_SEL_MASK;
	rw = val & TIMER_RW_MASK;
	mode = val & TIMER_MODE_MASK;

	if (sel == TIMER_SEL_READBACK) {
		return pit_readback(vpit, val);
	}

	if (rw != TIMER_LATCH) {
		if (rw != TIMER_16BIT) {
			printf("vpit unsupported rw: 0x%x\n", rw);
			return (-1);
		}

		/*
		 * Counter mode is not affected when issuing a
		 * latch command.
		 */
		if (mode != TIMER_INTTC &&
		    !PERIODIC_MODE(mode & ~TIMER_MODE_DONT_CARE_MASK) &&
		    mode != TIMER_SWSTROBE) {
			printf("vpit unsupported mode: 0x%x\n", mode);
			return (-1);
		}
	}

	channel = sel >> 6;
	c = &vpit->channel[channel];

	if (rw == TIMER_LATCH) {
		pit_update_counter(vpit, c, true, &delta_ticks);
	} else {
		if (mode == (TIMER_MODE_DONT_CARE_MASK | TIMER_RATEGEN) ||
		    mode == (TIMER_MODE_DONT_CARE_MASK | TIMER_SQWAVE)) {
			mode &= ~TIMER_MODE_DONT_CARE_MASK;
		}

		c->mode = mode;
		c->nullcnt = true;
		c->crbyte = 0;	/* control word must be written first */
		c->olbyte = 0;	/* reset latch after reprogramming */
		/* XXX reset frbyte? */

		if (channel == 0) {
			pit_timer_stop_cntr0(vpit, NULL);
		}
	}

	return (0);
}

static int
vpit_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		uint32_t *eax, void *arg)
{
	struct vpit *vpit = ctx->vpit;
	struct channel *c;
	uint8_t val;
	int error = 0;

	if (bytes != 1) {
		warnx("vpit invalid operation size: %d bytes", bytes);
		return -1;
	}

	val = *eax;

	if (port == TIMER_MODE) {
		if (in) {
			warnx("invalid in op @ io port 0x%x", port);
			return -1;
		}

		VPIT_LOCK();
		error = vpit_update_mode(vpit, val);
		VPIT_UNLOCK();

		return error;
	}

	/* counter ports */
	if (port < TIMER_CNTR0 || port > TIMER_CNTR2) {
		warnx("invalid %s op @ io port 0x%x", in ? "in" : "out", port);
		return -1;
	}

	c = &vpit->channel[port - TIMER_CNTR0];

	VPIT_LOCK();

	if (in) {
		if (c->slatched) {
			/*
			 * Return the status byte if latched
			 */
			*eax = c->status;
			c->slatched = false;
		} else {
			/*
			 * The spec says that once the output latch is completely
			 * read it should revert to "following" the counter. Use
			 * the free running counter for this case (i.e. Linux
			 * TSC calibration). Assuming the access mode is 16-bit,
			 * toggle the MSB/LSB bit on each read.
			 */
			if (c->olbyte == 0) {
				uint16_t tmp;
				uint64_t delta_ticks;

				tmp = pit_update_counter(vpit, c, false, &delta_ticks);

				if (c->frbyte)
					tmp >>= 8;
				tmp &= 0xff;
				*eax = tmp;
				c->frbyte ^= 1;
			} else {
				*eax = c->ol[--c->olbyte];
			}
		}
	} else {	/* out */
		if (c->crbyte == 2) {
			/* keep nullcnt */
			c->crbyte = 0;
		}

		c->cr[c->crbyte++] = *eax;

		if (c->crbyte == 2) {
			if (PERIODIC_MODE(c->mode) && pit_cr_val(c->cr) == 1) {
				/* illegal value */
				c->cr[0] = 0;
				c->crbyte = 0;
				error = -1;
				goto done;
			}

			c->frbyte = 0;
			c->nullcnt = true;

			/* Start an interval timer for channel 0 */
			if (port == TIMER_CNTR0) {
				pit_timer_start_cntr0(vpit);
			} else {
				/*
				 * For channel 1 & 2, load the value into CE
				 * immediately.
				 *
				 * XXX
				 * On real hardware, in periodic mode, CE doesn't
				 * get updated until the end of the current cycle
				 * or half-cycle.
				 */
				pit_load_ce(c);
			}
		}
	}

done:
	VPIT_UNLOCK();

	return error;
}

static int
vpit_nmisc_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		uint32_t *eax, void *arg)
{
	struct vpit *vpit = ctx->vpit;
	struct channel *c;

	/* XXX GATE2 control is not emulated */

	if (in) {
		c = &vpit->channel[2];

		VPIT_LOCK();

		if (vpit_get_out(vpit, 2, ticks_elapsed_since(&c->start_ts))) {
			*eax = TMR2_OUT_STS;
		} else {
			*eax = 0;
		}

		VPIT_UNLOCK();
	} else
		printf("out instr on NMI port (0x%u) not supported\n",
				NMISC_PORT);

	return (0);
}

int
vpit_init(struct vmctx *ctx)
{
	struct vpit *vpit;
	struct vpit_timer_arg *arg;
	int error = 0;
	int i;

	vpit = calloc(1U, sizeof(*vpit));

	if (vpit == NULL) {
		error = -ENOMEM;
		goto done;
	}

	vpit->vm = ctx;

	VPIT_LOCK();

	for (i = 0; i < nitems(vpit_timer_arg); i++) {
		arg = &vpit_timer_arg[i];
		arg->vpit = vpit;
		arg->channel_num = 0; /* only counter 0 uses a timer */
		arg->active = false;
	}

	ctx->vpit = vpit;

	VPIT_UNLOCK();

done:
	return error;
}

/*
 * Assume this function is called when only the
 * dangling timer callback can grab the vPIT lock.
 */
void
vpit_deinit(struct vmctx *ctx)
{
	struct vpit *vpit;

	VPIT_LOCK();

	vpit = ctx->vpit;

	if (vpit == NULL)
		goto done;

	ctx->vpit = NULL;
	pit_timer_stop_cntr0(vpit, NULL);
	memset(vpit_timer_arg, 0, sizeof(vpit_timer_arg));

done:
	VPIT_UNLOCK();

	if (vpit)
		free(vpit);
}

INOUT_PORT(vpit_counter0, TIMER_CNTR0, IOPORT_F_INOUT, vpit_handler);
INOUT_PORT(vpit_counter1, TIMER_CNTR1, IOPORT_F_INOUT, vpit_handler);
INOUT_PORT(vpit_counter2, TIMER_CNTR2, IOPORT_F_INOUT, vpit_handler);
INOUT_PORT(vpit_cwr, TIMER_MODE, IOPORT_F_OUT, vpit_handler);
INOUT_PORT(nmi, NMISC_PORT, IOPORT_F_INOUT, vpit_nmisc_handler);
