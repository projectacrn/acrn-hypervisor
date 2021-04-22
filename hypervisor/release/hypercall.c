/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <x86/guest/vm.h>

int32_t hcall_setup_sbuf(__unused struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return -EPERM;
}

int32_t hcall_setup_hv_npk_log(__unused struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return -EPERM;
}

int32_t hcall_get_hw_info(__unused struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return -EPERM;
}

int32_t hcall_profiling_ops(__unused struct acrn_vm *vm, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return -EPERM;
}
