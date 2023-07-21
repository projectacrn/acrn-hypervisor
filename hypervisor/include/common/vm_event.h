/*
 * Copyright (C) 2019-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_EVENT_H
#define VM_EVENT_H

#include <types.h>
#include <acrn_common.h>

int32_t init_vm_event(struct acrn_vm *vm, uint64_t *hva);
int32_t send_vm_event(struct acrn_vm *vm, struct vm_event *event);

#endif /* VM_EVENT_H */
