/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef _DM_KMSG_H_
#define _DM_KMSG_H_

#define DM_BUF    4096
#define KERN_NODE "/dev/kmsg"
#define KMSG_FMT  "dm_run:"

void write_kmsg(const char *log, ...);

#endif /* _DM_KMSG_H_ */
