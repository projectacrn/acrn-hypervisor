/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <vm.h>

int32_t hcall_debug(__unused struct acrn_vm *vm, __unused uint64_t param1, __unused uint64_t param2,
			__unused uint64_t hypcall_id)
{
	return -EPERM;
}
