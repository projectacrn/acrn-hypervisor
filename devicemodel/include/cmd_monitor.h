/*
 * Copyright (C) 2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _CMD_MONITOR_H_
#define _CMD_MONITOR_H_

int init_cmd_monitor(struct vmctx *ctx);
void deinit_cmd_monitor(void);
int acrn_parse_cmd_monitor(char *arg);
#endif
