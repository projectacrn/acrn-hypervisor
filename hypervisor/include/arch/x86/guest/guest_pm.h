/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GUEST_PM_H
#define GUEST_PM_H

void vm_setup_cpu_state(struct vm *vm);
int vm_load_pm_s_state(struct vm *vm);
int validate_pstate(struct vm *vm, uint64_t perf_ctl);
void register_pm1ab_handler(struct vm *vm);

#endif /* PM_H */
