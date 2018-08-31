/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/*
 * WatchDog Timer (WDT): emulate i6300esb PCI wdt Intel SOC devices,
 * used to monitor guest OS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>

#include "vmmapi.h"
#include "mevent.h"
#include "pci_core.h"

#define WDT_REG_BAR_SIZE		0x10

#define PCI_VENDOR_ID_INTEL		0x8086
#define PCI_DEVICE_ID_INTEL_ESB		0x25ab

#define ESB_CONFIG_REG			0x60 /* Config register*/
#define ESB_LOCK_REG			0x68 /* WDT lock register*/

/* Memory mapped registers */
#define ESB_TIMER1_REG	0x00 /* Timer1 value after each reset */
#define ESB_TIMER2_REG	0x04 /* Timer2 value after each reset */
#define ESB_RELOAD_REG	0x0c /* Reload register */

#define ESB_WDT_ENABLE	(0x01 << 1)   /* Enable WDT */
#define ESB_WDT_LOCK	(0x01 << 0)   /* Lock (nowayout) */

#define ESB_WDT_REBOOT	(0x01 << 5)   /* Enable reboot on timeout */
#define ESB_WDT_RELOAD	(0x01 << 8)   /* Ping/kick dog  */
#define	ESB_WDT_TIMEOUT	(0x01 << 9)   /* WDT timeout happened? */

/* Per i6300esb spec, in default watchdog timer prescaler
 * the 20-bit Preload Value is loaded into bits 34:15 of the
 * main down counter. The resulting timer clock is the PCI
 * Clock (33 MHz) divided by 2^^15 . The approximate clock
 * generated is 1 KHz, so right shift 10 bits of preload value
 * to get the exact seconds for this timer.
 */
#define TIMER_TO_SECONDS(val)	(val >> 10)

/* Magic constants */
#define ESB_UNLOCK1	0x80   /* Step 1 to unlock reset registers  */
#define ESB_UNLOCK2	0x86   /* Step 2 to unlock reset registers  */

#define WDT_TIMER_SIG			0x55AA

/* the default 20-bit preload value */
#define DEFAULT_MAX_TIMER_VAL		0x000FFFFF

/* stage timer is set to 60s after reset, then left shift 10 bits of seconds
 * to transform to the preload value of the watchdog timer which run in
 * 1KHz clock.
 */
#define	DEFAULT_RESET_TIMER_VAL		(60 << 10)

/* for debug */
/* #define WDT_DEBUG */
#ifdef WDT_DEBUG
static FILE * dbg_file;
#define DPRINTF(format, args...) \
do { fprintf(dbg_file, format, args); fflush(dbg_file); } while (0)
#else
#define DPRINTF(format, arg...)
#endif

struct info_wdt {
	bool reboot_enabled;/* "reboot" on wdt out */

	bool locked;        /* If true, enabled field cannot be changed. */
	bool wdt_enabled;   /* If true, watchdog is enabled. */

	bool timer_created;
	timer_t wdt_timerid;

	uint32_t timer1_val;
	uint32_t timer2_val;
	int stage;          /* stage 1 or 2. */

	int unlock_state;   /* unlock states 0 -> 1 -> 2  */
};

/* Whether watchdog is timeout. This info should cross reset. So
 * we didn't add to struct info_wdt.
 */
static int wdt_timeout = 0;

static struct info_wdt	wdt_state;

static void start_wdt_timer(void);

/*
 * WDT timer, start when guest OS start watchdog service; and re-start for
 * each dog-kick / ping action if time out, it will trigger reboot or other
 * action to guest OS
 */
static void
wdt_expired_thread(union sigval v)
{
	DPRINTF("wdt timer out! id=0x%x, stage=%d, reboot=%d\n",
		v.sival_int, wdt_state.stage, wdt_state.reboot_enabled);

	if (wdt_state.stage == 1) {
		wdt_state.stage = 2;
		start_wdt_timer();
	} else {
		if (wdt_state.reboot_enabled) {
			wdt_state.stage = 1;
			wdt_timeout = 1;

			/* watchdog timer out, set the uos to reboot */
			vm_set_suspend_mode(VM_SUSPEND_FULL_RESET);
			mevent_notify();
		} else {
			/* if not need reboot, just loop timer */
			wdt_state.stage = 1;
			start_wdt_timer();
		}
	}
}

static void
stop_wdt_timer(void)
{
	struct itimerspec timer_val;

	DPRINTF("%s: timer_created=%d\n", __func__, wdt_state.timer_created);

	if (!wdt_state.timer_created)
		return;

	memset(&timer_val, 0, sizeof(struct itimerspec));
	timer_settime(wdt_state.wdt_timerid, 0, &timer_val, NULL);
}

static void
delete_wdt_timer(void)
{
	if (!wdt_state.timer_created)
		return;

	DPRINTF("%s: timer %ld deleted\n", __func__,
		(uint64_t)wdt_state.wdt_timerid);

	timer_delete(wdt_state.wdt_timerid);
	wdt_state.timer_created = false;
}

static void
reset_wdt_timer(int seconds)
{
	struct itimerspec timer_val;

	DPRINTF("%s: time=%d\n", __func__, seconds);
	memset(&timer_val, 0, sizeof(struct itimerspec));
	timer_settime(wdt_state.wdt_timerid, 0, &timer_val, NULL);

	timer_val.it_value.tv_sec = seconds;
	if (timer_settime(wdt_state.wdt_timerid, 0, &timer_val, NULL) == -1) {
		perror("timer_settime failed.\n");
		timer_delete(wdt_state.wdt_timerid);
		wdt_state.timer_created = 0;
		exit(-1);
	}
}

static void
start_wdt_timer(void)
{
	int seconds;
	struct sigevent sig_evt;
	struct itimerspec timer_val;

	if (!wdt_state.wdt_enabled)
		return;

	if (wdt_state.stage == 1)
		seconds = TIMER_TO_SECONDS(wdt_state.timer1_val);
	else
		seconds = TIMER_TO_SECONDS(wdt_state.timer2_val);

	DPRINTF("%s: created=%d, time=%d\n", __func__,
			wdt_state.timer_created, seconds);
	memset(&sig_evt, 0, sizeof(struct sigevent));
	if (wdt_state.timer_created) {
		reset_wdt_timer(seconds);
		return;
	}

	sig_evt.sigev_value.sival_int = WDT_TIMER_SIG;
	sig_evt.sigev_notify = SIGEV_THREAD;
	sig_evt.sigev_notify_function = wdt_expired_thread;

	if (timer_create(CLOCK_REALTIME, &sig_evt,
		&wdt_state.wdt_timerid) == -1) {
		perror("timer_create failed.\n");
		exit(-1);
	}

	memset(&timer_val, 0, sizeof(struct itimerspec));
	timer_val.it_value.tv_sec = seconds;

	if (timer_settime(wdt_state.wdt_timerid, 0, &timer_val, NULL) == -1) {
		perror("timer_settime failed.\n");
		timer_delete(wdt_state.wdt_timerid);
		exit(-1);
	}

	wdt_state.timer_created = true;
}

static int
pci_wdt_cfg_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		 int offset, int bytes, uint32_t *rv)
{
	int need_cfg = 1;

	DPRINTF("%s: offset = %x, len = %d\n", __func__, offset, bytes);

	if (offset == ESB_LOCK_REG && bytes == 1) {
		*rv = (wdt_state.locked ? ESB_WDT_LOCK : 0) |
				(wdt_state.wdt_enabled ? ESB_WDT_ENABLE : 0);

		need_cfg = 0;
	}

	return need_cfg;
}

static int
pci_wdt_cfg_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		  int offset, int bytes, uint32_t val)
{
	bool old_flag;
	int need_cfg = 1;

	DPRINTF("%s: offset = %x, len = %d, val = 0x%x\n",
		__func__, offset, bytes, val);

	if (offset == ESB_CONFIG_REG && bytes == 2) {
		wdt_state.reboot_enabled = ((val & ESB_WDT_REBOOT) == 0);
		need_cfg = 0;

	} else if (offset == ESB_LOCK_REG && bytes == 1) {
		if (!wdt_state.locked) {
			wdt_state.locked = ((val & ESB_WDT_LOCK) != 0);
			old_flag = wdt_state.wdt_enabled;
			wdt_state.wdt_enabled = ((val & ESB_WDT_ENABLE) != 0);

			if (!old_flag && wdt_state.wdt_enabled) {
				wdt_state.stage = 1;
				start_wdt_timer();
			} else if (!wdt_state.wdt_enabled)
				stop_wdt_timer();
		}

		need_cfg = 0;
	}

	return need_cfg;
}

static void
pci_wdt_bar_write(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		  int baridx, uint64_t offset, int size, uint64_t value)
{
	assert(baridx == 0);

	DPRINTF("%s: addr = 0x%x, val = 0x%x, size=%d\n",
		__func__, (int) offset, (int)value, size);

	if (offset == ESB_RELOAD_REG) {
		assert(size == 2);

		if (value == ESB_UNLOCK1)
			wdt_state.unlock_state = 1;
		else if ((value == ESB_UNLOCK2)
			&& (wdt_state.unlock_state == 1))
			wdt_state.unlock_state = 2;
		else if (wdt_state.unlock_state == 2) {
			if (value & ESB_WDT_RELOAD) {
				wdt_state.stage = 1;
				start_wdt_timer();
			}

			/* write ES_WDT_TIMEOUT bit clear wdt timeout */
			if (value & ESB_WDT_TIMEOUT) {
				DPRINTF("%s: timeout cleaned\n\r", __func__);
				wdt_timeout = 0;
			}

			wdt_state.unlock_state = 0;
		}
	} else if (wdt_state.unlock_state == 2) {
		if (offset == ESB_TIMER1_REG)
			wdt_state.timer1_val = value & DEFAULT_MAX_TIMER_VAL;
		else if (offset == ESB_TIMER2_REG)
			wdt_state.timer2_val = value & DEFAULT_MAX_TIMER_VAL;

		wdt_state.unlock_state = 0;
	}
}

uint64_t
pci_wdt_bar_read(struct vmctx *ctx, int vcpu, struct pci_vdev *dev,
		 int baridx, uint64_t offset, int size)
{
	uint64_t ret = 0;

	assert(baridx == 0);
	DPRINTF("%s: addr = 0x%x, size=%d\n\r", __func__, (int) offset, size);

	if (offset == ESB_RELOAD_REG) {
		assert(size == 2);

		DPRINTF("%s: timeout: %d\n\r", __func__, wdt_timeout);
		if (wdt_timeout != 0)
			ret |= ESB_WDT_TIMEOUT;

		if (wdt_state.stage == 1)
			ret |= ESB_WDT_RELOAD;
	}

	return ret;
}

static int
pci_wdt_init(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	/*the wdt just has one inistance */
	if (wdt_state.reboot_enabled && wdt_state.timer1_val) {
		perror("wdt can't be created twice, please check!");
		return -1;
	}

	/* init wdt state info */
	wdt_state.reboot_enabled = true;
	wdt_state.locked = false;
	wdt_state.timer_created = false;
	wdt_state.wdt_enabled = false;

	wdt_state.stage = 1;
	wdt_state.timer1_val = DEFAULT_MAX_TIMER_VAL;
	wdt_state.timer2_val = DEFAULT_MAX_TIMER_VAL;
	wdt_state.unlock_state = 0;

	pci_emul_alloc_bar(dev, 0, PCIBAR_MEM32, WDT_REG_BAR_SIZE);

	/* initialize config space */
	pci_set_cfgdata16(dev, PCIR_VENDOR, PCI_VENDOR_ID_INTEL);
	pci_set_cfgdata16(dev, PCIR_DEVICE, PCI_DEVICE_ID_INTEL_ESB);
	pci_set_cfgdata8(dev, PCIR_CLASS, PCIC_BASEPERIPH);
	pci_set_cfgdata8(dev, PCIR_SUBCLASS, PCIS_BASEPERIPH_OTHER);

#ifdef WDT_DEBUG
	dbg_file = fopen("/tmp/wdt_log", "w+");
#endif

	DPRINTF("%s: iobar =0x%lx, size=%ld\n", __func__,
			dev->bar[0].addr, dev->bar[0].size);

	return 0;
}

static void
pci_wdt_deinit(struct vmctx *ctx, struct pci_vdev *dev, char *opts)
{
	delete_wdt_timer();
	memset(&wdt_state, 0, sizeof(wdt_state));
}

/* stop/reset watchdog will be invoked during guest enter/exit S3.
 * We stop watchdog timer when guest enter S3 to avoid watchdog trigger
 * guest reset when guest is in S3 state.
 *
 * We reset watchdog with a long peroid (2 * 60s) during guest exit
 * from S3 to handle system hang before watchdog in guest kernel start to
 * work.
 */
void
vm_stop_watchdog(struct vmctx *ctx)
{
	stop_wdt_timer();
}

void
vm_reset_watchdog(struct vmctx *ctx)
{
	wdt_state.stage = 1;
	wdt_state.timer1_val = DEFAULT_RESET_TIMER_VAL;
	wdt_state.timer2_val = DEFAULT_RESET_TIMER_VAL;
	wdt_state.unlock_state = 0;

	start_wdt_timer();
}

struct pci_vdev_ops pci_ops_wdt = {
	.class_name	= "wdt-i6300esb",
	.vdev_init	= pci_wdt_init,
	.vdev_deinit = pci_wdt_deinit,
	.vdev_cfgwrite = pci_wdt_cfg_write,
	.vdev_cfgread = pci_wdt_cfg_read,
	.vdev_barwrite	= pci_wdt_bar_write,
	.vdev_barread	= pci_wdt_bar_read
};

DEFINE_PCI_DEVTYPE(pci_ops_wdt);
