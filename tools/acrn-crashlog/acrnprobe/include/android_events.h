/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ANDROID_EVENTS_H__
#define __ANDROID_EVENTS_H__
#include "load_conf.h"

extern char *loop_dev;

void refresh_vm_history(struct sender_t *sender,
			void (*fn)(char*, struct vm_t *));

#endif
