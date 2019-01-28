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
#include <assert.h>
#include <stdbool.h>

#include "vmmapi.h"
#include "mevent.h"
#include "pci_core.h"
#include "timer.h"

#define WDT_REG_BAR_SIZE		0x10

#define PCI_VENDOR_ID_INTEL		0x8086
#define PCI_DEVICE_ID_INTEL_ESB		0x25ab

#define ESB_CONFIG_REG			0x60 /* Config register*/
#define ESB_LOCK_REG			0x68 /* WDT lock register*/

/* Memory mapped registers */
#define ESB_TIMER1_REG	0x00 /* Timer1 value after each reset */
#define ESB_TIMER2_REG	0x04 /* Timer2 value after each reset */
#define ESB_GIS_REG	0x08 /* General Interrupt Status register */
#define ESB_RELOAD_REG	0x0c /* Reload register */

#define ESB_WDT_ENABLE	(0x01 << 1)   /* Enable WDT */
#define ESB_WDT_LOCK	(0x01 << 0)   /* Lock (nowayout) */

#define ESB_WDT_INT_ACT	(0x01 << 0)   /* WDT INT is active */
#define ESB_WDT_INT_MSK	0x3           /* WDT INT TYPE mask */

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
	struct acrn_timer timer;

	bool reboot_enabled;/* "reboot" on wdt out */
	bool intr_enabled;  /* "intr" on wdt stage 1 time out */
	bool intr_active;   /* interrupt is active */

	bool locked;        /* If true, enabled field cannot be changed. */
	bool wdt_enabled;   /* If true, watchdog is enabled. */
	bool wdt_armed;

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
wdt_expired_handler(void *arg)
{
	struct pci_vdev *dev = (struct pci_vdev *)arg;

	DPRINTF("wdt timer out! stage=%d, reboot=%d\n",
		wdt_state.stage, wdt_state.reboot_enabled);

	if (wdt_state.stage == 1) {
		if (wdt_state.intr_enabled) {

			if (pci_msi_enabled(dev))
				pci_generate_msi(dev, 0);
			else
				pci_lintr_assert(dev);

			wdt_state.intr_active = true;
		}

		wdt_state.stage = 2;
		start_wdt_timer();
	} else {
		if (wdt_state.reboot_enabled) {
			wdt_state.stage = 1;
			wdt_timeout = 1;

			/* watchdog timer out, set the uos to reboot */
			vm_set_suspend_mode(VM_SUSPEND_SYSTEM_RESET);
			notify_vmloop_thread();
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

	DPRINTF("%s: wdt_armed=%d\n", __func__, wdt_state.wdt_armed);

	if (!wdt_state.wdt_armed)
		return;

	memset(&timer_val, 0, sizeof(struct itimerspec));
	acrn_timer_settime(&wdt_state.timer, &timer_val);
	wdt_state.wdt_armed = false;
}

static void
start_wdt_timer(void)
{
	int seconds;
	struct itimerspec timer_val;

	if (!wdt_state.wdt_enabled)
		return;

	if (wdt_state.stage == 1)
		seconds = TIMER_TO_SECONDS(wdt_state.timer1_val);
	else
		seconds = TIMER_TO_SECONDS(wdt_state.timer2_val);

	DPRINTF("%s: armed=%d, time=%d\n", __func__,
			wdt_state.wdt_armed, seconds);

	memset(&timer_val, 0, sizeof(struct itimerspec));
	timer_val.it_value.tv_sec = seconds;

	if (acrn_timer_settime(&wdt_state.timer, &timer_val) == -1) {
		perror("WDT timerfd_settime failed.\n");
		wdt_state.wdt_armed = false;
		return;
	}

	wdt_state.wdt_armed = true;
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
		wdt_state.intr_enabled = ((val & ESB_WDT_INT_MSK) == 0);
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

	if (offset == ESB_GIS_REG) {
		if ((value & ESB_WDT_INT_ACT) == 1) {

			wdt_state.intr_active = false;

			if ((wdt_state.intr_enabled == true)
					&& (dev->lintr.state == ASSERTED)) {
				pci_lintr_deassert(dev);
				DPRINTF("%s: intr deasserted\n\r", __func__);
			}
		}
	} else if (offset == ESB_RELOAD_REG) {
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

	if (offset == ESB_GIS_REG) {
		if ((wdt_state.intr_enabled == true)
				&& (wdt_state.intr_active == true))
			ret |= ESB_WDT_INT_ACT;

	} else if (offset == ESB_RELOAD_REG) {
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
		perror("wdt can't be initialized twice, please check!");
		return -1;
	}

	/* init wdt state info */
	wdt_state.timer.clockid = CLOCK_MONOTONIC;
	if (acrn_timer_init(&wdt_state.timer, wdt_expired_handler, dev) != 0) {
		return -1;
	}

	wdt_state.reboot_enabled = true;
	wdt_state.intr_enabled = false;
	wdt_state.intr_active = false;
	wdt_state.locked = false;
	wdt_state.wdt_armed = false;
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

	pci_emul_add_msicap(dev, 1);
	pci_lintr_request(dev);

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
	acrn_timer_deinit(&wdt_state.timer);

	memset(&wdt_state, 0, sizeof(wdt_state));

	pci_lintr_release(dev);

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
