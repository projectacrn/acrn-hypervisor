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
struct cpu_cx_data* get_target_cx(struct vm *vm, uint8_t cn);

#endif /* PM_H */
