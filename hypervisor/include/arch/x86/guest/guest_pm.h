/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GUEST_PM_H
#define GUEST_PM_H

void vm_setup_cpu_state(struct acrn_vm *vm);
int32_t vm_load_pm_s_state(struct acrn_vm *vm);
int32_t validate_pstate(const struct acrn_vm *vm, uint64_t perf_ctl);
void register_pm1ab_handler(struct acrn_vm *vm);
void register_rt_vm_pm1a_ctl_handler(struct acrn_vm *vm);

#endif /* PM_H */
