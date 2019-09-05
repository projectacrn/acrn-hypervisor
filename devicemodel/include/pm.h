/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef	_DM_INCLUDE_PM_
#define	_DM_INCLUDE_PM_

#define	PWR_EVENT_NOTIFY_IOC	0x1
#define	PWR_EVENT_NOTIFY_PWR_BT	0x2
#define	PWR_EVENT_NOTIFY_UART	0x3

int wait_for_resume(struct vmctx *ctx);
int vm_resume(struct vmctx *ctx);
int vm_monitor_resume(void *arg);
int vm_monitor_query(void *arg);

#endif
