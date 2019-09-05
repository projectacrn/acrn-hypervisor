/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
#include "mevent.h"
#include "monitor.h"

#define POWER_BUTTON_NAME	"power_button"
#define POWER_BUTTON_ACPI_DRV	"/sys/bus/acpi/drivers/button/LNXPWRBN:00/"
#define POWER_BUTTON_INPUT_DIR POWER_BUTTON_ACPI_DRV"input"
#define POWER_BUTTON_PNP0C0C_DRV "/sys/bus/acpi/drivers/button/PNP0C0C:00/"
#define POWER_BUTTON_PNP0C0C_DIR POWER_BUTTON_PNP0C0C_DRV"input"

static struct mevent *input_evt0;
static int pwrbtn_fd = -1;
static bool monitor_run;

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
		inject_power_button_event(arg);
}

static int
vm_stop_handler(void *arg)
{
	if (!arg)
		return -EINVAL;

	inject_power_button_event(arg);
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

void
power_button_init(struct vmctx *ctx)
{
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
}

void
power_button_deinit(struct vmctx *ctx)
{
	if (input_evt0 != NULL) {
		mevent_delete_close(input_evt0);
		input_evt0 = NULL;
		pwrbtn_fd = -1;
	}
}
