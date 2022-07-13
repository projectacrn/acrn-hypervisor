/*
 * Copyright (C) 2018-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include "vmmapi.h"
#include "log.h"

static pthread_cond_t suspend_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t suspend_mutex = PTHREAD_MUTEX_INITIALIZER;

int
wait_for_resume(struct vmctx *ctx)
{
	pthread_mutex_lock(&suspend_mutex);
	while (vm_get_suspend_mode() == VM_SUSPEND_SUSPEND) {
		pthread_cond_wait(&suspend_cond, &suspend_mutex);
	}
	pthread_mutex_unlock(&suspend_mutex);

	return 0;
}

int
vm_resume(struct vmctx *ctx)
{
	pthread_mutex_lock(&suspend_mutex);
	pr_info("%s: setting VM state to %s\n", __func__, vm_state_to_str(VM_SUSPEND_NONE));
	vm_set_suspend_mode(VM_SUSPEND_NONE);
	pthread_cond_signal(&suspend_cond);
	pthread_mutex_unlock(&suspend_mutex);

	return 0;
}

int
vm_monitor_resume(void *arg)
{
	struct vmctx *ctx = (struct vmctx *)arg;
	vm_resume(ctx);

	return 0;
}

int
vm_monitor_query(void *arg)
{
	return vm_get_suspend_mode();
}
