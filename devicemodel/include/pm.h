/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef	_DM_INCLUDE_PM_
#define	_DM_INCLUDE_PM_

int wait_for_resume(struct vmctx *ctx);
int vm_resume(struct vmctx *ctx);
int vm_monitor_resume(void *arg);
int vm_monitor_query(void *arg);

#endif
