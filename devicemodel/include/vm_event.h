/*
 * Copyright (C) 2019-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_EVENT_H
#define VM_EVENT_H

#include <types.h>
#include "vmmapi.h"

int vm_event_init(struct vmctx *ctx);
int vm_event_deinit(void);
int dm_send_vm_event(struct vm_event *event);
uint32_t get_dm_vm_event_overrun_count(void);

#endif /* VM_EVENT_H */
