/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2013 Tycho Nightingale <tycho.nightingale@pluribusnetworks.com>
 * Copyright (c) 2013 Neel Natu <neel@freebsd.org>
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
 *
 * $FreeBSD$
 */

#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <err.h>
#include <sysexits.h>
#include <string.h>
#include <unistd.h>

#include "vmmapi.h"
#include "mem.h"
#include "timer.h"
#include "hpet.h"
#include "acpi_hpet.h"

#define	HPET_FREQ	(16777216)		/* 16.7 (2^24) Mhz */
#define	FS_PER_S	(1000000000000000UL)

/* Timer N Configuration and Capabilities Register */
#define	HPET_TCAP_RO_MASK	(HPET_TCAP_INT_ROUTE 	|		\
				 HPET_TCAP_FSB_INT_DEL	|		\
				 HPET_TCAP_SIZE		|		\
				 HPET_TCAP_PER_INT)

/*
 * HPET requires at least 3 timers and up to 32 timers per block.
 */
#define	VHPET_NUM_TIMERS	(8)

#define	VHPET_LOCK()												\
	do {															\
		int err;													\
		err = pthread_mutex_lock(&vhpet_mtx);						\
		if (err)													\
			errx(EX_SOFTWARE, "pthread_mutex_lock returned %s",		\
					strerror(err));									\
	} while (0)

#define	VHPET_UNLOCK()												\
	do {															\
		int err;													\
		err = pthread_mutex_unlock(&vhpet_mtx);						\
		if (err)													\
			errx(EX_SOFTWARE, "pthread_mutex_unlock returned %s",	\
					strerror(err));									\
	} while (0)

#define vhpet_ts_to_ticks(ts)	ts_to_ticks(HPET_FREQ, ts)
/* won't overflow since the max value of ticks is 2^32 */
#define vhpet_ticks_to_ts(tk, ts)	ticks_to_ts(HPET_FREQ, tk, ts)

#define vhpet_tmr(v, n)	(&(v)->timer[n].tmrlst[(v)->timer[n].tmridx].t)
#define vhpet_tmrarg(v, n)	(&(v)->timer[n].tmrlst[(v)->timer[n].tmridx].a)

#define ts_is_zero(ts)	timespeccmp(ts, &zero_ts.it_value, ==)
#define ts_set_zero(ts)	do { *(ts) = zero_ts.it_value; } while (0)

/*
 * Debug printf
 */
static int hpet_debug;
#define DPRINTF(params)	do { if (hpet_debug) printf params; } while (0)
#define WPRINTF(params)	(printf params)


static struct mem_range vhpet_mr = {
	.name = "vhpet",
	.base = VHPET_BASE,
	.size = VHPET_SIZE,
	.flags = MEM_F_RW
};

struct vhpet_timer_arg {
	struct vhpet	*vhpet;
	int	timer_num;
	bool	running;
};

struct vhpet {
	struct vmctx	*vm;
	bool	inited;

	uint64_t	config;		/* Configuration */
	uint64_t	isr;		/* Interrupt Status */
	uint32_t	countbase;	/* HPET counter base value */
	struct timespec	countbase_ts;	/* uptime corresponding to base value */

	struct {
		uint64_t	cap_config;	/* Configuration */
		uint64_t	msireg;		/* FSB interrupt routing */
		uint32_t	compval;	/* Comparator */
		uint32_t	comprate;
		struct timespec	expts;	/* time when counter==compval */
		struct {
			struct acrn_timer	t;
			struct vhpet_timer_arg	a;
		} tmrlst[3];
		int	tmridx;
	} timer[VHPET_NUM_TIMERS];
};


/* one vHPET per VM */
static pthread_mutex_t vhpet_mtx = PTHREAD_MUTEX_INITIALIZER;

static const struct itimerspec zero_ts = { 0 };


/* one vHPET per VM */
static struct vhpet *
vhpet_instance(void)
{
	static struct vhpet __vhpet;

	return &__vhpet;
}

const uint64_t
vhpet_capabilities(void)
{
	static uint64_t cap = 0;

	if (cap == 0) {
		cap |= 0x8086 << 16;			/* vendor id */
		cap |= (VHPET_NUM_TIMERS - 1) << 8;	/* number of timers */
		cap |= 1;				/* revision */
		cap &= ~HPET_CAP_COUNT_SIZE;		/* 32-bit timer */

		cap &= 0xffffffff;
		cap |= (FS_PER_S / HPET_FREQ) << 32;	/* tick period in fs */
	}

	return cap;
}

static inline bool
vhpet_counter_enabled(struct vhpet *vhpet)
{
	return ((vhpet->config & HPET_CNF_ENABLE) != 0);
}

static inline bool
vhpet_timer_msi_enabled(struct vhpet *vhpet, int n)
{
	const uint64_t msi_enable = HPET_TCAP_FSB_INT_DEL | HPET_TCNF_FSB_EN;

	return ((vhpet->timer[n].cap_config & msi_enable) == msi_enable);
}

static inline int
vhpet_timer_ioapic_pin(struct vhpet *vhpet, int n)
{
	/*
	 * If the timer is configured to use MSI then treat it as if the
	 * timer is not connected to the ioapic.
	 */
	if (vhpet_timer_msi_enabled(vhpet, n))
		return 0;

	return ((vhpet->timer[n].cap_config & HPET_TCNF_INT_ROUTE) >> 9);
}

static uint32_t
vhpet_counter(struct vhpet *vhpet, struct timespec *nowptr)
{
	uint32_t val;
	struct timespec now, delta;

	val = vhpet->countbase;

	if (vhpet_counter_enabled(vhpet)) {
		if (clock_gettime(CLOCK_REALTIME, &now))
			errx(EX_SOFTWARE, "clock_gettime returned: %s", strerror(errno));

		/* delta = now - countbase_ts */
		if (timespeccmp(&now, &vhpet->countbase_ts, <)) {
			warnx("vhpet counter going backwards");
			vhpet->countbase_ts = now;
		}

		delta = now;
		timespecsub(&delta, &vhpet->countbase_ts);
		val += vhpet_ts_to_ticks(&delta);

		if (nowptr != NULL)
			*nowptr = now;
	} else {
		/*
		 * The timespec corresponding to the 'countbase' is
		 * meaningless when the counter is disabled. Warn if
		 * the caller wants to use it.
		 */
		if (nowptr) {
			warnx("vhpet unexpected nowptr");
			if (clock_gettime(CLOCK_REALTIME, nowptr))
				errx(EX_SOFTWARE, "clock_gettime returned: %s",
						strerror(errno));
		}
	}

	return val;
}

static void
vhpet_timer_clear_isr(struct vhpet *vhpet, int n)
{
	int pin;

	if (vhpet->isr & (1 << n)) {
		pin = vhpet_timer_ioapic_pin(vhpet, n);

		if (pin)
			vm_set_gsi_irq(vhpet->vm, pin, GSI_SET_LOW);
		else
			warnx("vhpet t%d intr asserted without a valid intr route", n);

		vhpet->isr &= ~(1 << n);
	}
}

static inline bool
vhpet_periodic_timer(struct vhpet *vhpet, int n)
{
	return ((vhpet->timer[n].cap_config & HPET_TCNF_TYPE) != 0);
}

static inline bool
vhpet_timer_interrupt_enabled(struct vhpet *vhpet, int n)
{
	return ((vhpet->timer[n].cap_config & HPET_TCNF_INT_ENB) != 0);
}

static inline bool
vhpet_timer_enabled(struct vhpet *vhpet, int n)
{
	/* The timer is enabled when at least one of the two bits is set */
	return vhpet_timer_interrupt_enabled(vhpet, n) ||
	       vhpet_periodic_timer(vhpet, n);
}

static inline bool
vhpet_timer_running(struct vhpet *vhpet, int n)
{
	return vhpet_tmrarg(vhpet, n)->running;
}

static inline bool
vhpet_timer_edge_trig(struct vhpet *vhpet, int n)
{
	return (!vhpet_timer_msi_enabled(vhpet, n) &&
	        (vhpet->timer[n].cap_config & HPET_TCNF_INT_TYPE) == 0);
}

static void
vhpet_timer_interrupt(struct vhpet *vhpet, int n)
{
	int pin;

	/* If interrupts are not enabled for this timer then just return. */
	if (!vhpet_timer_interrupt_enabled(vhpet, n))
		return;

	/*
	 * If a level triggered interrupt is already asserted then just return.
	 */
	if (vhpet->isr & (1 << n)) {
		if (!vhpet_timer_msi_enabled(vhpet, n) &&
		    !vhpet_timer_edge_trig(vhpet, n)) {
			DPRINTF(("hpet t%d intr is already asserted\n", n));
			return;
		} else {
			warnx("vhpet t%d intr asserted in %s mode", n,
					vhpet_timer_msi_enabled(vhpet, n) ?
					"msi" : "edge-triggered");
			vhpet->isr &= ~(1 << n);
		}
	}

	if (vhpet_timer_msi_enabled(vhpet, n)) {
		vm_lapic_msi(vhpet->vm, vhpet->timer[n].msireg >> 32,
				vhpet->timer[n].msireg & 0xffffffff);
		return;
	}

	pin = vhpet_timer_ioapic_pin(vhpet, n);

	if (pin == 0) {
		DPRINTF(("hpet t%d intr is not routed to ioapic\n", n));
		return;
	}

	if (vhpet_timer_edge_trig(vhpet, n)) {
		vm_set_gsi_irq(vhpet->vm, pin, GSI_RAISING_PULSE);
	} else {
		vhpet->isr |= 1 << n;
		vm_set_gsi_irq(vhpet->vm, pin, GSI_SET_HIGH);
	}
}

static void
vhpet_timer_handler(void *a, uint64_t nexp)
{
	int n;
	struct vhpet *vhpet;
	struct vhpet_timer_arg *arg;
	struct timespec now;
	struct itimerspec tmrts;
	uint64_t newexp;

	arg = a;
	vhpet = arg->vhpet;
	n = arg->timer_num;

	DPRINTF(("hpet t%d(%p) fired\n", n, arg));

	VHPET_LOCK();

	/* Bail if timer was destroyed */
	if (!vhpet->inited)
		goto done;

	/* Bail if timer was stopped */
	if (!arg->running) {
		DPRINTF(("hpet t%d(%p) already stopped\n", n, arg));
		if (!ts_is_zero(&vhpet->timer[n].expts)) {
			warnx("vhpet t%d stopped with an expiration time", n);
			ts_set_zero(&vhpet->timer[n].expts);
		}
		goto done;
	} else if (arg != vhpet_tmrarg(vhpet, n)) {
		warnx("vhpet t%d observes a stale timer arg", n);
		goto done;
	}

	vhpet_timer_interrupt(vhpet, n);

	if (clock_gettime(CLOCK_REALTIME, &now))
		errx(EX_SOFTWARE, "clock_gettime returned: %s", strerror(errno));

	if (acrn_timer_gettime(vhpet_tmr(vhpet, n), &tmrts))
		errx(EX_SOFTWARE, "acrn_timer_gettime returned: %s", strerror(errno));

	/* One-shot mode has a periodicity of 2^32 ticks */
	if (ts_is_zero(&tmrts.it_interval))
		warnx("vhpet t%d has no periodicity", n);

	/*
	 * The actual expiration time will be slightly later than expts.
	 *
	 * This may cause spurious interrupts when stopping a timer but
	 * at least interrupts won't be completely lost.
	 */
	timespecadd(&tmrts.it_value, &now);
	vhpet->timer[n].expts = tmrts.it_value;

	/*
	 * Catch any remaining expirations that happened after being
	 * last consumed by the mevent_dispatch thread.
	 */
	if (read(vhpet_tmr(vhpet, n)->fd, &newexp, sizeof(newexp)) > 0)
		nexp += newexp;

	/*
	 * Periodic timer updates 'compval' upon expiration.
	 * Try to keep 'compval' as up-to-date as possible.
	 */
	vhpet->timer[n].compval += nexp * vhpet->timer[n].comprate;

done:
	VHPET_UNLOCK();
	return;
}

static void
vhpet_adjust_compval(struct vhpet *vhpet, int n, const struct timespec *now)
{
	uint32_t compval, comprate, compnext;
	struct timespec delta;
	uint64_t delta_ticks;

	compval = vhpet->timer[n].compval;
	comprate = vhpet->timer[n].comprate;

	if (!comprate || timespeccmp(&vhpet->timer[n].expts, now, >=))
		return;

	/* delta = now - expts */
	delta = *now;
	timespecsub(&delta, &vhpet->timer[n].expts);
	delta_ticks = vhpet_ts_to_ticks(&delta);

	/*
	 * Calculate the comparator value to be used for the next periodic
	 * interrupt.
	 *
	 * In this scenario 'counter' is ahead of 'compval' by at least
	 * 'comprate'. To find the next value to program into the
	 * accumulator we divide 'delta_ticks', the number space between
	 * 'compval + comprate' and 'counter', into 'comprate' sized units.
	 * The 'compval' is rounded up such that it stays "ahead" of
	 * 'counter'.
	 *
	 * There's a slight chance read() in vhpet_timer_handler() had
	 * already accomplished some of this just prior to calling this
	 * function.
	 */
	compnext = compval + (delta_ticks / comprate + 1) * comprate;

	vhpet->timer[n].compval = compnext;
}

static void
vhpet_stop_timer(struct vhpet *vhpet, int n, const struct timespec *now,
		bool adj_compval)
{
	struct vhpet_timer_arg *arg;

	if (!vhpet_timer_running(vhpet, n))
		return;

	if (ts_is_zero(&vhpet->timer[n].expts))
		warnx("vhpet t%d is running without an expiration time", n);

	DPRINTF(("hpet t%d stopped\n", n));

	arg = vhpet_tmrarg(vhpet, n);
	arg->running = false;

	/* Cancel the existing timer */
	if (acrn_timer_settime(vhpet_tmr(vhpet, n), &zero_ts))
		errx(EX_SOFTWARE, "acrn_timer_settime returned: %s", strerror(errno));

	if (++vhpet->timer[n].tmridx == nitems(vhpet->timer[n].tmrlst))
		vhpet->timer[n].tmridx = 0;

	if (vhpet_timer_running(vhpet, n)) {
		warnx("vhpet t%d timer %d is still running",
				n, vhpet->timer[n].tmridx);
		vhpet_stop_timer(vhpet, n, &zero_ts.it_value, false);
	}

	/*
	 * If the timer was scheduled to expire in the past but hasn't
	 * had a chance to execute yet then trigger the timer interrupt
	 * here. Failing to do so will result in a missed timer interrupt
	 * in the guest. This is especially bad in one-shot mode because
	 * the next interrupt has to wait for the counter to wrap around.
	 */
	if (!ts_is_zero(&vhpet->timer[n].expts)) {
		if (timespeccmp(&vhpet->timer[n].expts, now, <)) {
			DPRINTF(("hpet t%d interrupt triggered after "
					 "stopping timer\n", n));
			if (adj_compval)
				vhpet_adjust_compval(vhpet, n, now);
			vhpet_timer_interrupt(vhpet, n);
		}

		ts_set_zero(&vhpet->timer[n].expts);
	}
}

static void
vhpet_start_timer(struct vhpet *vhpet, int n, uint32_t counter,
		const struct timespec *now, bool adj_compval)
{
	struct itimerspec ts;
	uint32_t delta;
	struct vhpet_timer_arg *arg;

	vhpet_stop_timer(vhpet, n, now, adj_compval);

	DPRINTF(("hpet t%d started\n", n));

	/*
	 * It is the guest's responsibility to make sure that the
	 * comparator value is not in the "past". The hardware
	 * doesn't have any belt-and-suspenders to deal with this
	 * so we don't either.
	 */
	delta = vhpet->timer[n].compval - counter;
	vhpet_ticks_to_ts(delta, &ts.it_value);
	timespecadd(&ts.it_value, now);

	if (vhpet->timer[n].comprate != 0)
		vhpet_ticks_to_ts(vhpet->timer[n].comprate, &ts.it_interval);
	else	/* It takes 2^32 ticks to wrap around */
		vhpet_ticks_to_ts(1ULL << 32, &ts.it_interval);

	arg = vhpet_tmrarg(vhpet, n);
	arg->running = true;

	/* Arm the new timer */
	if (acrn_timer_settime_abs(vhpet_tmr(vhpet, n), &ts))
		errx(EX_SOFTWARE, "acrn_timer_settime_abs returned: %s",
				strerror(errno));

	vhpet->timer[n].expts = ts.it_value;
}

static void
vhpet_restart_timer(struct vhpet *vhpet, int n, bool adj_compval)
{
	uint32_t counter;
	struct timespec now;

	/*
	 * Restart the specified timer based on the current value of
	 * the main counter.
	 */
	counter = vhpet_counter(vhpet, &now);
	vhpet_start_timer(vhpet, n, counter, &now, adj_compval);
}

static void
vhpet_start_counting(struct vhpet *vhpet)
{
	int i;

	if (clock_gettime(CLOCK_REALTIME, &vhpet->countbase_ts))
		errx(EX_SOFTWARE, "clock_gettime returned: %s", strerror(errno));

	/* Restart the timers based on the main counter base value */
	for (i = 0; i < VHPET_NUM_TIMERS; i++) {
		if (vhpet_timer_enabled(vhpet, i))
			vhpet_start_timer(vhpet, i, vhpet->countbase,
					&vhpet->countbase_ts, true);
		else if (vhpet_timer_running(vhpet, i)) {
			warnx("vhpet t%d's timer is disabled but running", i);
			vhpet_stop_timer(vhpet, i, &zero_ts.it_value, false);
		}
	}
}

static void
vhpet_stop_counting(struct vhpet *vhpet, uint32_t counter,
		const struct timespec *now)
{
	int i;

	/* Update the main counter base value */
	vhpet->countbase = counter;

	for (i = 0; i < VHPET_NUM_TIMERS; i++) {
		if (vhpet_timer_enabled(vhpet, i))
			vhpet_stop_timer(vhpet, i, now, true);
		else if (vhpet_timer_running(vhpet, i)) {
			warnx("vhpet t%d's timer is disabled but running", i);
			vhpet_stop_timer(vhpet, i, &zero_ts.it_value, false);
		}
	}
}

static inline void
update_register(uint64_t *const regptr, const uint64_t data,
		const uint64_t mask)
{
	*regptr &= ~mask;
	*regptr |= (data & mask);
}

static void
vhpet_timer_update_config(struct vhpet *vhpet, int n, uint64_t data,
		uint64_t mask)
{
	int old_pin, new_pin;
	uint32_t allowed_irqs;
	uint64_t oldval, newval;
	struct timespec now;

	if (vhpet_timer_msi_enabled(vhpet, n) ||
	    vhpet_timer_edge_trig(vhpet, n)) {
		if (vhpet->isr & (1 << n)) {
			warnx("vhpet t%d intr asserted in %s mode", n,
					vhpet_timer_msi_enabled(vhpet, n) ?
					"msi" : "edge-triggered");
			vhpet->isr &= ~(1 << n);
		}
	}

	old_pin = vhpet_timer_ioapic_pin(vhpet, n);
	oldval = vhpet->timer[n].cap_config;

	newval = oldval;
	update_register(&newval, data, mask);
	newval &= ~(HPET_TCAP_RO_MASK | HPET_TCNF_32MODE);
	newval |= oldval & HPET_TCAP_RO_MASK;

	if (newval == oldval)
		return;

	vhpet->timer[n].cap_config = newval;
	DPRINTF(("hpet t%d cap_config set to 0x%016lx\n", n, newval));

	if ((oldval ^ newval) & (HPET_TCNF_TYPE | HPET_TCNF_INT_ENB)) {
		if (!vhpet_periodic_timer(vhpet, n))
			vhpet->timer[n].comprate = 0;

		if (vhpet_counter_enabled(vhpet)) {
			/*
			 * Stop the timer if both bits are now cleared
			 *
			 * Else, restart the timer if:
			 *   - The timer was stopped, or
			 *   - HPET_TCNF_TYPE is being toggled
			 *
			 * Else, no-op
			 *   - Timer remains in periodic mode
			 */
			if (!vhpet_timer_enabled(vhpet, n)) {
				if (clock_gettime(CLOCK_REALTIME, &now))
					errx(EX_SOFTWARE, "clock_gettime returned: %s",
							strerror(errno));
				vhpet_stop_timer(vhpet, n, &now, true);
			} else if (!(oldval & (HPET_TCNF_TYPE | HPET_TCNF_INT_ENB)) ||
			           ((oldval ^ newval) & HPET_TCNF_TYPE))
				vhpet_restart_timer(vhpet, n, true);
		}
	}

	/*
	 * Validate the interrupt routing in the HPET_TCNF_INT_ROUTE field.
	 * If it does not match the bits set in HPET_TCAP_INT_ROUTE then set
	 * it to the default value of 0.
	 */
	allowed_irqs = vhpet->timer[n].cap_config >> 32;
	new_pin = vhpet_timer_ioapic_pin(vhpet, n);

	if (new_pin != 0 && (allowed_irqs & (1 << new_pin)) == 0) {
		WPRINTF(("hpet t%d configured invalid irq %d, "
		         "allowed_irqs 0x%08x\n", n, new_pin, allowed_irqs));
		new_pin = 0;
		vhpet->timer[n].cap_config &= ~HPET_TCNF_INT_ROUTE;
	}

	/*
	 * If the timer's ISR bit is set then clear it in the following cases:
	 * - interrupt is disabled
	 * - interrupt type is changed from level to edge or fsb.
	 * - interrupt routing is changed
	 *
	 * This is to ensure that this timer's level triggered interrupt does
	 * not remain asserted forever.
	 */
	if (vhpet->isr & (1 << n)) {
		if (!old_pin) {
			warnx("vhpet t%d intr asserted without a valid intr route", n);
			vhpet->isr &= ~(1 << n);
		} else if (!vhpet_timer_interrupt_enabled(vhpet, n) ||
		    vhpet_timer_msi_enabled(vhpet, n) ||
		    vhpet_timer_edge_trig(vhpet, n) ||
		    new_pin != old_pin) {
			DPRINTF(("hpet t%d isr cleared due to "
			         "configuration change\n", n));
			vm_set_gsi_irq(vhpet->vm, old_pin, GSI_SET_LOW);
			vhpet->isr &= ~(1 << n);
		}
	}
}

static int
vhpet_mmio_write(struct vhpet *vhpet, int vcpuid, uint64_t gpa, uint64_t *wval,
		int size)
{
	uint64_t data, mask, oldval, val64;
	uint32_t isr_clear_mask, old_compval, old_comprate, counter;
	struct timespec now, *nowptr;
	int offset, i;

	offset = gpa - VHPET_BASE;

	/* Accesses to the HPET should be 4 or 8 bytes wide */
	switch (size) {
	case 8:
		mask = 0xffffffffffffffff;
		data = *wval;
		break;
	case 4:
		mask = 0xffffffff;
		data = *wval;
		if ((offset & 0x4) != 0) {
			mask <<= 32;
			data <<= 32;
		}
		break;
	default:
		WPRINTF(("hpet invalid mmio write: "
		         "offset 0x%08x, size %d\n", offset, size));
		goto done;
	}

	/* Access to the HPET should be naturally aligned to its width */
	if (offset & (size - 1)) {
		WPRINTF(("hpet invalid mmio write: "
		         "offset 0x%08x, size %d\n", offset, size));
		goto done;
	}

	if (offset == HPET_CONFIG || offset == HPET_CONFIG + 4) {
		/*
		 * Get the most recent value of the counter before updating
		 * the 'config' register. If the HPET is going to be disabled
		 * then we need to update 'countbase' with the value right
		 * before it is disabled.
		 */
		nowptr = vhpet_counter_enabled(vhpet) ? &now : NULL;
		counter = vhpet_counter(vhpet, nowptr);
		oldval = vhpet->config;
		update_register(&vhpet->config, data, mask);

		/*
		 * LegacyReplacement Routing is not supported so clear the
		 * bit along with the reserved bits explicitly.
		 */
		vhpet->config &= HPET_CNF_ENABLE;

		if ((oldval ^ vhpet->config) & HPET_CNF_ENABLE) {
			if (vhpet_counter_enabled(vhpet)) {
				vhpet_start_counting(vhpet);
				DPRINTF(("hpet enabled\n"));
			} else {
				vhpet_stop_counting(vhpet, counter, &now);
				DPRINTF(("hpet disabled\n"));
			}
		}
		goto done;
	}

	if (offset == HPET_ISR || offset == HPET_ISR + 4) {
		/* Top 32 bits are reserved */
		isr_clear_mask = vhpet->isr & data;
		for (i = 0; i < VHPET_NUM_TIMERS; i++) {
			if ((isr_clear_mask & (1 << i)) != 0) {
				DPRINTF(("hpet t%d isr cleared\n", i));
				vhpet_timer_clear_isr(vhpet, i);
			}
		}
		goto done;
	}

	if (offset == HPET_MAIN_COUNTER || offset == HPET_MAIN_COUNTER + 4) {
		/* Zero-extend the counter to 64-bits before updating it */
		val64 = vhpet_counter(vhpet, NULL);
		update_register(&val64, data, mask);
		vhpet->countbase = val64;
		if (vhpet_counter_enabled(vhpet))
			vhpet_start_counting(vhpet);
		goto done;
	}

	for (i = 0; i < VHPET_NUM_TIMERS; i++) {
		if (offset == HPET_TIMER_CAP_CNF(i) ||
		    offset == HPET_TIMER_CAP_CNF(i) + 4) {
			vhpet_timer_update_config(vhpet, i, data, mask);
			break;
		}

		if (offset == HPET_TIMER_COMPARATOR(i) ||
		    offset == HPET_TIMER_COMPARATOR(i) + 4) {
			old_compval = vhpet->timer[i].compval;
			old_comprate = vhpet->timer[i].comprate;

			if (vhpet_periodic_timer(vhpet, i)) {
				/*
				 * In periodic mode, writes to the comparator
				 * change the 'compval' register only if the
				 * HPET_TCNF_VAL_SET bit is set in the config
				 * register.
				 */
				val64 = vhpet->timer[i].comprate;
				update_register(&val64, data, mask);
				vhpet->timer[i].comprate = val64;

				if ((vhpet->timer[i].cap_config &
				     HPET_TCNF_VAL_SET) != 0)
					vhpet->timer[i].compval = val64;
			} else {
				if (vhpet->timer[i].comprate) {
					warnx("vhpet t%d's comprate is %u in non-periodic mode"
							" - should be 0", i, vhpet->timer[i].comprate);
					vhpet->timer[i].comprate = 0;
				}
				val64 = vhpet->timer[i].compval;
				update_register(&val64, data, mask);
				vhpet->timer[i].compval = val64;
			}

			vhpet->timer[i].cap_config &= ~HPET_TCNF_VAL_SET;

			if (vhpet->timer[i].compval != old_compval ||
			    vhpet->timer[i].comprate != old_comprate) {
				if (vhpet_counter_enabled(vhpet) &&
				    vhpet_timer_enabled(vhpet, i))
					vhpet_restart_timer(vhpet, i, false);
			}
			break;
		}

		if (offset == HPET_TIMER_FSB_VAL(i) ||
		    offset == HPET_TIMER_FSB_ADDR(i)) {
			update_register(&vhpet->timer[i].msireg, data, mask);
			break;
		}
	}

	if (i >= VHPET_NUM_TIMERS)
		WPRINTF(("hpet invalid mmio write: "
		         "offset 0x%08x, size %d\n", offset, size));

done:
	return 0;
}

static int
vhpet_mmio_read(struct vhpet *vhpet, int vcpuid, uint64_t gpa, uint64_t *rval,
		int size)
{
	int offset, i;
	uint64_t data;

	offset = gpa - VHPET_BASE;

	/*
	 * Accesses to the HPET should be:
	 *   - 4 or 8 bytes wide
	 *   - naturally aligned to its width
	 */
	if ((size != 4 && size != 8) || (offset & (size - 1))) {
		WPRINTF(("hpet invalid mmio read: "
		         "offset 0x%08x, size %d\n", offset, size));
		data = 0;
		goto done;
	}

	if (offset == HPET_CAPABILITIES || offset == HPET_CAPABILITIES + 4) {
		data = vhpet_capabilities();
		goto done;
	}

	if (offset == HPET_CONFIG || offset == HPET_CONFIG + 4) {
		data = vhpet->config;
		goto done;
	}

	if (offset == HPET_ISR || offset == HPET_ISR + 4) {
		data = vhpet->isr;
		goto done;
	}

	if (offset == HPET_MAIN_COUNTER || offset == HPET_MAIN_COUNTER + 4) {
		data = vhpet_counter(vhpet, NULL);
		goto done;
	}

	for (i = 0; i < VHPET_NUM_TIMERS; i++) {
		if (offset == HPET_TIMER_CAP_CNF(i) ||
		    offset == HPET_TIMER_CAP_CNF(i) + 4) {
			data = vhpet->timer[i].cap_config;
			break;
		}

		if (offset == HPET_TIMER_COMPARATOR(i) ||
		    offset == HPET_TIMER_COMPARATOR(i) + 4) {
			data = vhpet->timer[i].compval;
			break;
		}

		if (offset == HPET_TIMER_FSB_VAL(i) ||
		    offset == HPET_TIMER_FSB_ADDR(i)) {
			data = vhpet->timer[i].msireg;
			break;
		}
	}

	if (i >= VHPET_NUM_TIMERS) {
		WPRINTF(("hpet invalid mmio read: "
		         "offset 0x%08x, size %d\n", offset, size));
		data = 0;
	}

done:
	if (size == 4) {
		if (offset & 0x4)
			data >>= 32;
	}

	*rval = data;
	return 0;
}

static int
vhpet_handler(struct vmctx *ctx, int vcpu, int dir, uint64_t addr,
		int size, uint64_t *val, void *arg1, long arg2)
{
	struct vhpet *vhpet = arg1;
	int error;

	VHPET_LOCK();

	if (!vhpet->inited) {
		error = -EINVAL;
		goto done;
	}

	error = ((dir == MEM_F_READ) ? vhpet_mmio_read : vhpet_mmio_write)(
				vhpet, vcpu, addr, val, size);

done:
	VHPET_UNLOCK();
	return error;
}

static void
vhpet_deinit_timers(struct vhpet *vhpet)
{
	int i, j;
	struct acrn_timer *tmr;

	for (i = 0; i < VHPET_NUM_TIMERS; i++) {
		for (j = 0; j < nitems(vhpet->timer[i].tmrlst); j++) {
			tmr = &vhpet->timer[i].tmrlst[j].t;
			acrn_timer_deinit(tmr);
		}
	}
}

int
vhpet_init(struct vmctx *ctx)
{
	int error = 0, pincount, i, j;
	struct vhpet *vhpet;
	uint64_t allowed_irqs;
	struct vhpet_timer_arg *arg;
	struct acrn_timer *tmr;

	vhpet = vhpet_instance();

	VHPET_LOCK();

	if (vhpet->inited) {
		WPRINTF(("hpet already initialized!\n"));
		error = -EINVAL;
		goto done;
	}

	memset(vhpet, 0, sizeof(*vhpet));
	vhpet->vm = ctx;

	pincount = VIOAPIC_RTE_NUM;

	if (pincount >= 32)
		allowed_irqs = 0xff000000;	/* irqs 24-31 */
	else if (pincount >= 20)
		allowed_irqs = 0xf << (pincount - 4);	/* 4 upper irqs */
	else
		allowed_irqs = 0;

	/*
	 * Initialize HPET timer hardware state.
	 */
	for (i = 0; i < VHPET_NUM_TIMERS; i++) {
		vhpet->timer[i].cap_config = allowed_irqs << 32;
		vhpet->timer[i].cap_config |= HPET_TCAP_PER_INT;
		vhpet->timer[i].cap_config |= HPET_TCAP_FSB_INT_DEL;
		vhpet->timer[i].compval = 0xffffffff;

		for (j = 0; j < nitems(vhpet->timer[i].tmrlst); j++) {
			arg = &vhpet->timer[i].tmrlst[j].a;
			arg->vhpet = vhpet;
			arg->timer_num = i;

			tmr = &vhpet->timer[i].tmrlst[j].t;
			tmr->clockid = CLOCK_REALTIME;
			error = acrn_timer_init(tmr, vhpet_timer_handler, arg);

			if (error) {
				vhpet_deinit_timers(vhpet);
				goto done;
			}
		}
	}

	vhpet_mr.handler = vhpet_handler;
	vhpet_mr.arg1 = vhpet;
	vhpet_mr.arg2 = 0;

	error = register_mem(&vhpet_mr);

	if (error) {
		vhpet_deinit_timers(vhpet);
		goto done;
	}

	vhpet->inited = true;

done:
	VHPET_UNLOCK();
	return error;
}

void
vhpet_deinit(struct vmctx *ctx)
{
	struct vhpet *vhpet;

	vhpet = vhpet_instance();

	VHPET_LOCK();

	if (!vhpet->inited)
		goto done;

	vhpet_deinit_timers(vhpet);
	unregister_mem(&vhpet_mr);

	vhpet->inited = false;

done:
	VHPET_UNLOCK();
}
