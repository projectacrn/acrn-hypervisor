/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <time.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <errno.h>
#include "mevent.h"
#include "acrnctl.h"
#include "acrn_mngr.h"
#include "ioc.h"

#define TIMER_LIST_FILE "/opt/acrn/conf/timer_list"

/* load/store_timer_list to file to keep timers if SOS poweroff */
int load_timer_list(void)
{
	return -1;
}

static int active_all_vms(void)
{
	return -1;
}

#define SOS_LCS_SOCK		"sos-lcs"
#define ACRND_NAME		"acrnd"
static int acrnd_fd = -1;

unsigned get_sos_wakeup_reason(void)
{
	return 0;
}

static void handle_timer_req(struct mngr_msg *msg, int client_fd, void *param)
{
}

static int store_timer_list(void)
{
	return -1;
}

static void handle_acrnd_stop(struct mngr_msg *msg, int client_fd, void *param)
{
}

void handle_acrnd_resume(struct mngr_msg *msg, int client_fd, void *param)
{
}

static void handle_on_exit(void)
{
	store_timer_list();

	if (acrnd_fd > 0) {
		mngr_close(acrnd_fd);
		acrnd_fd = -1;
	}
}

int init_vm(void)
{
	unsigned int wakeup_reason;
	struct stat st;

	if (!stat(TIMER_LIST_FILE, &st))
		if (S_ISREG(st.st_mode))
			return load_timer_list();

	/* init all UOSs, according wakeup_reason */
	wakeup_reason = get_sos_wakeup_reason();

	if (wakeup_reason & CBC_WK_RSN_RTC)
		return load_timer_list();
	else {
		/* TODO: auto start UOSs */
		return active_all_vms();
	}
}

int main(int argc, char *argv[])
{
	/* create listening thread */
	acrnd_fd = mngr_open_un(ACRND_NAME, MNGR_SERVER);
	if (acrnd_fd < 0) {
		pdebug();
		return -1;
	}

	if (init_vm()) {
		pdebug();
		return -1;
	}

	unlink(TIMER_LIST_FILE);

	atexit(handle_on_exit);

	mngr_add_handler(acrnd_fd, ACRND_TIMER, handle_timer_req, NULL);
	mngr_add_handler(acrnd_fd, ACRND_STOP, handle_acrnd_stop, NULL);
	mngr_add_handler(acrnd_fd, ACRND_RESUME, handle_acrnd_resume, NULL);

	return 0;
}
