/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GUEST_PM_H
#define GUEST_PM_H

int32_t validate_pstate(const struct acrn_vm *vm, uint64_t perf_ctl);
void init_guest_pm(struct acrn_vm *vm);

#endif /* PM_H */
