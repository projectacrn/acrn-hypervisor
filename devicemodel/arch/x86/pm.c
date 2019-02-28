/*-
 * Copyright (c) 2013 Hudson River Trading LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include "vmmapi.h"
#include "acpi.h"
#include "inout.h"
#include "mevent.h"
#include "irq.h"
#include "lpc.h"
#include "monitor.h"

#define POWER_BUTTON_EVENT	116
#define POWER_BUTTON_NAME	"power_button"
static pthread_mutex_t pm_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mevent *power_button;
static sig_t old_power_handler;

static struct mevent *input_evt0;
static int pwrbtn_fd = -1;
static bool monitor_run;
/*
 * Reset Control register at I/O port 0xcf9.  Bit 2 forces a system
 * reset when it transitions from 0 to 1.  Bit 1 selects the type of
 * reset to attempt: 0 selects a "soft" reset, and 1 selects a "hard"
 * reset.
 */
static int
reset_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
	      uint32_t *eax, void *arg)
{
	int error;
	static uint8_t reset_control;

	if (bytes != 1)
		return -1;
	if (in)
		*eax = reset_control;
	else {
		reset_control = *eax;

		if (*eax & 0x8) {
			fprintf(stderr, "full reset\r\n");
			error = vm_suspend(ctx, VM_SUSPEND_FULL_RESET);
			assert(error ==0 || errno == EALREADY);
			mevent_notify();
			reset_control = 0;
		} else if (*eax & 0x4) {
			fprintf(stderr, "system reset\r\n");
			error = vm_suspend(ctx, VM_SUSPEND_SYSTEM_RESET);
			assert(error ==0 || errno == EALREADY);
			mevent_notify();
		}
	}
	return 0;
}
INOUT_PORT(reset_reg, 0xCF9, IOPORT_F_INOUT, reset_handler);

/*
 * ACPI's SCI is a level-triggered interrupt.
 */
static int sci_active;

static void
sci_assert(struct vmctx *ctx)
{
	if (sci_active)
		return;
	vm_set_gsi_irq(ctx, SCI_INT, GSI_SET_HIGH);
	sci_active = 1;
}

static void
sci_deassert(struct vmctx *ctx)
{
	if (!sci_active)
		return;
	vm_set_gsi_irq(ctx, SCI_INT, GSI_SET_LOW);
	sci_active = 0;
}

/*
 * Power Management 1 Event Registers
 *
 * The only power management event supported is a power button upon
 * receiving SIGTERM.
 */
static uint16_t pm1_enable, pm1_status;

#define	PM1_TMR_STS		0x0001
#define	PM1_BM_STS		0x0010
#define	PM1_GBL_STS		0x0020
#define	PM1_PWRBTN_STS		0x0100
#define	PM1_SLPBTN_STS		0x0200
#define	PM1_RTC_STS		0x0400
#define	PM1_WAK_STS		0x8000

#define	PM1_TMR_EN		0x0001
#define	PM1_GBL_EN		0x0020
#define	PM1_PWRBTN_EN		0x0100
#define	PM1_SLPBTN_EN		0x0200
#define	PM1_RTC_EN		0x0400

static void
sci_update(struct vmctx *ctx)
{
	int need_sci;

	/* See if the SCI should be active or not. */
	need_sci = 0;
	if ((pm1_enable & PM1_TMR_EN) && (pm1_status & PM1_TMR_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_GBL_EN) && (pm1_status & PM1_GBL_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_PWRBTN_EN) && (pm1_status & PM1_PWRBTN_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_SLPBTN_EN) && (pm1_status & PM1_SLPBTN_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_RTC_EN) && (pm1_status & PM1_RTC_STS))
		need_sci = 1;
	if (need_sci)
		sci_assert(ctx);
	else
		sci_deassert(ctx);
}

static int
pm1_status_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		   uint32_t *eax, void *arg)
{
	if (bytes != 2)
		return -1;

	pthread_mutex_lock(&pm_lock);
	if (in)
		*eax = pm1_status;
	else {
		/*
		 * Writes are only permitted to clear certain bits by
		 * writing 1 to those flags.
		 */
		pm1_status &= ~(*eax & (PM1_WAK_STS | PM1_RTC_STS |
		    PM1_SLPBTN_STS | PM1_PWRBTN_STS | PM1_BM_STS));
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);

	return 0;
}

void
pm_backto_wakeup(struct vmctx *ctx)
{
	/* According to ACPI 5.0 Table 4-16: bit 15, WAK_STS should be
	 * set when system trasition to the working state
	 */
	pm1_status |= PM1_WAK_STS;
}

static int
pm1_enable_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		   uint32_t *eax, void *arg)
{
	if (bytes != 2)
		return -1;

	pthread_mutex_lock(&pm_lock);
	if (in)
		*eax = pm1_enable;
	else {
		/*
		 * Only permit certain bits to be set.  We never use
		 * the global lock, but ACPI-CA whines profusely if it
		 * can't set GBL_EN.
		 */
		pm1_enable = *eax & (PM1_RTC_EN | PM1_PWRBTN_EN | PM1_GBL_EN);
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);

	return 0;
}
INOUT_PORT(pm1_status, PM1A_EVT_ADDR, IOPORT_F_INOUT, pm1_status_handler);
INOUT_PORT(pm1_enable, PM1A_EVT_ADDR + 2, IOPORT_F_INOUT, pm1_enable_handler);

static void
power_button_press_emulation(struct vmctx *ctx)
{
	pthread_mutex_lock(&pm_lock);
	if (!(pm1_status & PM1_PWRBTN_STS)) {
		pm1_status |= PM1_PWRBTN_STS;
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);
}

static void
power_button_handler(int signal, enum ev_type type, void *arg)
{
	if (arg)
		power_button_press_emulation(arg);
}

static void
input_event0_handler(int fd, enum ev_type type, void *arg)
{
	struct input_event ev;
	int rc;

	rc = read(fd, &ev, sizeof(ev));
	if (rc < 0 || rc != sizeof(ev))
		return;

	if (ev.code == POWER_BUTTON_EVENT && ev.value == 1)
		power_button_press_emulation(arg);
}

/*
 * Power Management 1 Control Register
 *
 * This is mostly unimplemented except that we wish to handle writes that
 * set SPL_EN to handle S5 (soft power off).
 */
static uint16_t pm1_control;

#define	PM1_SCI_EN	0x0001
#define	PM1_SLP_TYP	0x1c00
#define	PM1_SLP_EN	0x2000
#define	PM1_ALWAYS_ZERO	0xc003

static int
pm1_control_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		    uint32_t *eax, void *arg)
{
	int error;

	if (bytes != 2)
		return -1;
	if (in)
		*eax = pm1_control;
	else {
		/*
		 * Various bits are write-only or reserved, so force them
		 * to zero in pm1_control.  Always preserve SCI_EN as OSPM
		 * can never change it.
		 */
		pm1_control = (pm1_control & PM1_SCI_EN) |
		    (*eax & ~(PM1_SLP_EN | PM1_ALWAYS_ZERO));

		/*
		 * If SLP_EN is set, check for S5.  ACRN-DM's _S5_ method
		 * says that '5' should be stored in SLP_TYP for S5.
		 */
		if (*eax & PM1_SLP_EN) {
			if ((pm1_control & PM1_SLP_TYP) >> 10 == 5) {
				error = vm_suspend(ctx, VM_SUSPEND_POWEROFF);
				assert(error == 0 || errno == EALREADY);
			}

			if ((pm1_control & PM1_SLP_TYP) >> 10 == 3) {
				error = vm_suspend(ctx, VM_SUSPEND_SUSPEND);
				assert(error == 0 || errno == EALREADY);
			}
		}
	}
	return 0;
}
INOUT_PORT(pm1_control, PM1A_CNT_ADDR, IOPORT_F_INOUT, pm1_control_handler);
SYSRES_IO(PM1A_EVT_ADDR, 8);

static int
vm_stop_handler(void *arg)
{
	if (!arg)
		return -EINVAL;

	power_button_press_emulation(arg);
	return 0;
}

static int
vm_suspend_handler(void *arg)
{
	/*
	 * Invoke vm_stop_handler directly in here since suspend of UOS is
	 * set by UOS power button setting.
	 */
	return vm_stop_handler(arg);
}

static struct monitor_vm_ops vm_ops = {
	.stop = vm_stop_handler,
	.suspend = vm_suspend_handler,
};

/*
 * ACPI SMI Command Register
 *
 * This write-only register is used to enable and disable ACPI.
 */
static int
smi_cmd_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		uint32_t *eax, void *arg)
{
	assert(!in);
	if (bytes != 1)
		return -1;

	pthread_mutex_lock(&pm_lock);
	switch (*eax) {
	case ACPI_ENABLE:
		pm1_control |= PM1_SCI_EN;
		/*
		 * FIXME: ACPI_ENABLE/ACPI_DISABLE only impacts SCI_EN via SMI
		 * command register, not impact power button emulation. so need
		 * to remove all power button emulation from here.
		 */
		if (power_button == NULL) {

			/*
			 * TODO: For the SIGTERM, IOC mediator also needs to
			 * support it, and SIGTERM handler needs to be written
			 * as one common interface for both APCI power button
			 * and IOC mediator in future.
			 */
			power_button = mevent_add(SIGTERM, EVF_SIGNAL,
				power_button_handler, ctx, NULL, NULL);
			old_power_handler = signal(SIGTERM, SIG_IGN);
		}
		if (input_evt0 == NULL) {
			/*
			 * FIXME: check /sys/bus/acpi/devices/LNXPWRBN\:00/input to
			 * get input event node instead hardcode in here.
			 */
			pwrbtn_fd = open("/dev/input/event0", O_RDONLY);
			if (pwrbtn_fd < 0)
				fprintf(stderr, "open input event0 error=%d\n",
						errno);
			else
				input_evt0 = mevent_add(pwrbtn_fd, EVF_READ,
					input_event0_handler, ctx, NULL, NULL);
		}

		/*
		 * Suspend or shutdown UOS by acrnctl suspend and
		 * stop command.
		 */
		if (monitor_run == false) {
			if (monitor_register_vm_ops(&vm_ops, ctx,
						POWER_BUTTON_NAME) < 0)
				fprintf(stderr,
				"failed to register vm ops for power button\n");
			else
				monitor_run = true;
		}
		break;
	case ACPI_DISABLE:
		pm1_control &= ~PM1_SCI_EN;
		if (power_button != NULL) {
			mevent_delete(power_button);
			power_button = NULL;
			signal(SIGTERM, old_power_handler);
		}
		if (input_evt0 != NULL) {
			mevent_delete_close(input_evt0);
			input_evt0 = NULL;
			pwrbtn_fd = -1;
		}
		break;
	}
	pthread_mutex_unlock(&pm_lock);
	return 0;
}
INOUT_PORT(smi_cmd, SMI_CMD, IOPORT_F_OUT, smi_cmd_handler);
SYSRES_IO(SMI_CMD, 1);

void
sci_init(struct vmctx *ctx)
{
	/*
	 * Mark ACPI's SCI as level trigger and bump its use count
	 * in the PIRQ router.
	 */
	pci_irq_use(SCI_INT);
}
