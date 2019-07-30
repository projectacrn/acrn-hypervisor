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
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#include "vmmapi.h"
#include "acpi.h"
#include "inout.h"
#include "mevent.h"
#include "irq.h"
#include "lpc.h"
#include "monitor.h"

#define POWER_BUTTON_NAME	"power_button"
#define POWER_BUTTON_ACPI_DRV	"/sys/bus/acpi/drivers/button/LNXPWRBN:00/"
#define POWER_BUTTON_INPUT_DIR POWER_BUTTON_ACPI_DRV"input"
#define POWER_BUTTON_PNP0C0C_DRV "/sys/bus/acpi/drivers/button/PNP0C0C:00/"
#define POWER_BUTTON_PNP0C0C_DIR POWER_BUTTON_PNP0C0C_DRV"input"
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
	static uint8_t reset_control;

	if (bytes != 1)
		return -1;
	if (in)
		*eax = reset_control;
	else {
		reset_control = *eax;

		if (*eax & 0x8) {
			fprintf(stderr, "full reset\r\n");
			vm_suspend(ctx, VM_SUSPEND_FULL_RESET);
			mevent_notify();
			reset_control = 0;
		} else if (*eax & 0x4) {
			fprintf(stderr, "system reset\r\n");
			vm_suspend(ctx, VM_SUSPEND_SYSTEM_RESET);
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

/*
 * Power Management 1 Control Register
 *
 * This is mostly unimplemented except that we wish to handle writes that
 * set SPL_EN to handle S5 (soft power off).
 */
static uint16_t pm1_control;

static void
sci_update(struct vmctx *ctx)
{
	int need_sci;

	/*
	 * Followed ACPI spec, should trigger SMI if SCI_EN is zero.
	 * Return directly due to ACRN do not support SMI so far.
	 */
	if (!(pm1_control & VIRTUAL_PM1A_SCI_EN))
		return;

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
	printf("%s", "press power button\n");
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

	/*
	 * The input key defines in input-event-codes.h
	 * KEY_POWER 116 SC System Power Down
	 */
	if (ev.code == KEY_POWER && ev.value == 1)
		power_button_press_emulation(arg);
}

static int
pm1_control_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		    uint32_t *eax, void *arg)
{
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
		pm1_control = (pm1_control & VIRTUAL_PM1A_SCI_EN) |
		    (*eax & ~(VIRTUAL_PM1A_SLP_EN | VIRTUAL_PM1A_ALWAYS_ZERO));

		/*
		 * If SLP_EN is set, check for S5.  ACRN-DM's _S5_ method
		 * says that '5' should be stored in SLP_TYP for S5.
		 */
		if (*eax & VIRTUAL_PM1A_SLP_EN) {
			if ((pm1_control & VIRTUAL_PM1A_SLP_TYP) >> 10 == 5) {
				vm_suspend(ctx, VM_SUSPEND_POWEROFF);
			}

			if ((pm1_control & VIRTUAL_PM1A_SLP_TYP) >> 10 == 3) {
				vm_suspend(ctx, VM_SUSPEND_SUSPEND);
			}
		}
	}
	return 0;
}
INOUT_PORT(pm1_control, VIRTUAL_PM1A_CNT_ADDR, IOPORT_F_INOUT, pm1_control_handler);
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

static int
input_dir_filter(const struct dirent *dir)
{
	return !strncmp(dir->d_name, "input", 5);
}

static int
event_dir_filter(const struct dirent *dir)
{
	return !strncmp(dir->d_name, "event", 5);
}

static int
open_power_button_input_device(const char *drv, const char *dir)
{
	struct dirent **input_dirs = NULL;
	struct dirent **event_dirs = NULL;
	int ninput = 0;
	int nevent = 0;
	char path[256] = {0};
	char name[256] = {0};
	int rc, fd;

	if (access(drv, F_OK) != 0)
		return -1;
	/*
	 * Scan path to get inputN
	 * path is /sys/bus/acpi/drivers/button/LNXPWRBN:00/input
	 */
	ninput = scandir(dir, &input_dirs, input_dir_filter,
			alphasort);
	if (ninput < 0) {
		fprintf(stderr, "failed to scan power button %s\n",
				dir);
		goto err;
	} else if (ninput == 1) {
		rc = snprintf(path, sizeof(path), "%s/%s",
				dir, input_dirs[0]->d_name);
		if (rc < 0 || rc >= sizeof(path)) {
			fprintf(stderr, "failed to set power button path %d\n",
					rc);
			goto err_input;
		}

		/*
		 * Scan path to get eventN
		 * path is /sys/bus/acpi/drivers/button/LNXPWRBN:00/input/inputN
		 */
		nevent = scandir(path, &event_dirs, event_dir_filter,
				alphasort);
		if (nevent < 0) {
			fprintf(stderr, "failed to get power button event %s\n",
					path);
			goto err_input;
		} else if (nevent == 1) {

			/* Get the power button input event name */
			rc = snprintf(name, sizeof(name), "/dev/input/%s",
					event_dirs[0]->d_name);
			if (rc < 0 || rc >= sizeof(name)) {
				fprintf(stderr, "power button error %d\n", rc);
				goto err_input;
			}
		} else {
			fprintf(stderr, "power button event number error %d\n",
					nevent);
			goto err_event;
		}
	} else {
		fprintf(stderr, "power button input number error %d\n", nevent);
		goto err_input;
	}

	/* Open the input device */
	fd = open(name, O_RDONLY);
	if (fd > 0)
		printf("Watching power button on %s\n", name);

	while (nevent--)
		free(event_dirs[nevent]);
	free(event_dirs);
	while (ninput--)
		free(input_dirs[ninput]);
	free(input_dirs);
	return fd;

err_event:
	while (nevent--)
		free(event_dirs[nevent]);
	free(event_dirs);

err_input:
	while (ninput--)
		free(input_dirs[ninput]);
	free(input_dirs);

err:
	return -1;
}

static int
open_native_power_button()
{
	int fd;

	/*
	 * Open fixed power button firstly, if it can't be opened
	 * try to open control method power button.
	 */
	fd = open_power_button_input_device(POWER_BUTTON_ACPI_DRV,
			POWER_BUTTON_INPUT_DIR);
	if (fd < 0)
		return open_power_button_input_device(
				POWER_BUTTON_PNP0C0C_DRV,
				POWER_BUTTON_PNP0C0C_DIR);
	else
		return fd;
}

/*
 * ACPI SMI Command Register
 *
 * This write-only register is used to enable and disable ACPI.
 */
static int
smi_cmd_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
		uint32_t *eax, void *arg)
{
	if (in || (bytes != 1))
		return -1;

	pthread_mutex_lock(&pm_lock);
	switch (*eax & 0xFF) {
	case ACPI_ENABLE:
		pm1_control |= VIRTUAL_PM1A_SCI_EN;
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

			pwrbtn_fd = open_native_power_button();
			if (pwrbtn_fd < 0)
				fprintf(stderr, "open power button error=%d\n",
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
		pm1_control &= ~VIRTUAL_PM1A_SCI_EN;
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
