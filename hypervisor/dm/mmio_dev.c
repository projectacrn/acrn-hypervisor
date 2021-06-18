/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <util.h>
#include <acrn_hv_defs.h>
#include <asm/pgtable.h>
#include <asm/guest/vm.h>
#include <asm/guest/ept.h>

int32_t assign_mmio_dev(struct acrn_vm *vm, const struct acrn_mmiores *res)
{
	int32_t ret = -EINVAL;

	if (mem_aligned_check(res->base_gpa, PAGE_SIZE) &&
			mem_aligned_check(res->base_hpa, PAGE_SIZE) &&
			mem_aligned_check(res->size, PAGE_SIZE)) {
		ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, res->base_hpa,
				is_sos_vm(vm) ? res->base_hpa : res->base_gpa,
				res->size, EPT_RWX | EPT_UNCACHED);
		ret = 0;
	}

	return ret;
}

int32_t deassign_mmio_dev(struct acrn_vm *vm, const struct acrn_mmiores *res)
{
	int32_t ret = -EINVAL;

	if (mem_aligned_check(res->base_gpa, PAGE_SIZE) &&
			mem_aligned_check(res->base_hpa, PAGE_SIZE) &&
			mem_aligned_check(res->size, PAGE_SIZE)) {
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp,
				is_sos_vm(vm) ? res->base_hpa : res->base_gpa, res->size);
		ret = 0;
	}

	return ret;
}
