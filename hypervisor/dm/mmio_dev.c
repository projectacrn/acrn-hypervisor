/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <util.h>
#include <acrn_hv_defs.h>
#include <asm/pgtable.h>
#include <asm/guest/vm.h>
#include <asm/guest/ept.h>
#include <logmsg.h>


int32_t assign_mmio_dev(struct acrn_vm *vm, const struct acrn_mmiodev *mmiodev)
{
	int32_t i, ret = 0;
	const struct acrn_mmiores *res;

	for (i = 0; i < MMIODEV_RES_NUM; i++) {
		res = &mmiodev->res[i];
		if (mem_aligned_check(res->user_vm_pa, PAGE_SIZE) &&
			mem_aligned_check(res->host_pa, PAGE_SIZE) &&
			mem_aligned_check(res->size, PAGE_SIZE)) {
			ept_add_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, res->host_pa,
				is_service_vm(vm) ? res->host_pa : res->user_vm_pa,
				res->size, EPT_RWX | (res->mem_type & EPT_MT_MASK));
		} else {
			pr_err("%s invalid mmio res[%d] gpa:0x%lx hpa:0x%lx size:0x%lx",
				__FUNCTION__, i, res->user_vm_pa, res->host_pa, res->size);
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int32_t deassign_mmio_dev(struct acrn_vm *vm, const struct acrn_mmiodev *mmiodev)
{
	int32_t i, ret = 0;
	uint64_t gpa;
	const struct acrn_mmiores *res;

	for (i = 0; i < MMIODEV_RES_NUM; i++) {
		res = &mmiodev->res[i];
		gpa = is_service_vm(vm) ? res->host_pa : res->user_vm_pa;
		if (ept_is_valid_mr(vm, gpa, res->size)) {
			if (mem_aligned_check(gpa, PAGE_SIZE) &&
				mem_aligned_check(res->size, PAGE_SIZE)) {
				ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, gpa, res->size);
			} else {
				pr_err("%s invalid mmio res[%d] gpa:0x%lx hpa:0x%lx size:0x%lx",
					__FUNCTION__, i, res->user_vm_pa, res->host_pa, res->size);
				ret = -EINVAL;
				break;
			}
		}
	}

	return ret;
}
