/*
 * Project Acrn
 * Acrn-dm-monitor
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *
 * Author: TaoYuhong <yuhong.tao@intel.com>
 */

/* acrn-dm monitor APIS */

#ifndef MONITOR_H
#define MONITOR_H

int monitor_init(struct vmctx *ctx);
void monitor_close(void);

struct monitor_vm_ops {
	int (*stop) (void *arg);
	int (*resume) (void *arg);
	int (*suspend) (void *arg);
	int (*pause) (void *arg);
	int (*unpause) (void *arg);
	int (*query) (void *arg);
	int (*rescan)(void *arg, char *devargs);
};

int monitor_register_vm_ops(struct monitor_vm_ops *ops, void *arg,
			    const char *name);

/* helper functions for vm_ops callback developer */
unsigned get_wakeup_reason(void);
int set_wakeup_timer(time_t t);
int acrn_parse_intr_monitor(const char *opt);
int vm_monitor_blkrescan(void *arg, char *devargs);
#endif
