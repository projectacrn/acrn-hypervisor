/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hypercall.h>

#define ACRN_DBG_TRUSTY_HYCALL 6U

/**
 * @brief Switch vCPU state between Normal/Secure World.
 *
 * * The hypervisor uses this hypercall to do the world switch
 * * The hypervisor needs to:
 *   * save current world vCPU contexts, and load the next world
 *     vCPU contexts
 *   * update ``rdi``, ``rsi``, ``rdx``, ``rbx`` to next world
 *     vCPU contexts
 *
 * @param vcpu Pointer to VCPU data structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_world_switch(struct acrn_vcpu *vcpu)
{
	int32_t next_world_id = !(vcpu->arch.cur_context);

	if (next_world_id >= NR_WORLD) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"%s world_id %d exceed max number of Worlds\n",
			__func__, next_world_id);
		return -EINVAL;
	}

	if (vcpu->vm->sworld_control.flag.supported == 0UL) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Secure World is not supported!\n");
		return -EPERM;
	}

	if (vcpu->vm->sworld_control.flag.active == 0UL) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Trusty is not initialized!\n");
		return -EPERM;
	}

	switch_world(vcpu, next_world_id);
	return 0;
}

/**
 * @brief Initialize environment for Trusty-OS on a vCPU.
 *
 * * It is used by the User OS bootloader (``UOS_Loader``) to request ACRN
 *   to initialize Trusty
 * * The Trusty memory region range, entry point must be specified
 * * The hypervisor needs to save current vCPU contexts (Normal World)
 *
 * @param vcpu Pointer to vCPU data structure
 * @param param guest physical address. This gpa points to
 *              trusty_boot_param structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_initialize_trusty(struct acrn_vcpu *vcpu, uint64_t param)
{
	int32_t ret = 0;

	if (vcpu->vm->sworld_control.flag.supported == 0UL) {
		pr_err("Secure World is not supported!\n");
		ret = -EPERM;
	} else if (vcpu->vm->sworld_control.flag.active != 0UL) {
		pr_err("Trusty already initialized!\n");
		ret = -EPERM;
	} else if (vcpu->arch.cur_context != NORMAL_WORLD) {
		pr_err("%s, must initialize Trusty from Normal World!\n", __func__);
		ret = -EPERM;
	} else {
		if (!initialize_trusty(vcpu, param)) {
			ret = -ENODEV;
		} else {
			vcpu->vm->sworld_control.flag.active = 1UL;
		}
	}

	return ret;
}

/**
 * @brief Save/Restore Context of Secure World.
 *
 * @param vcpu Pointer to VCPU data structure
 *
 * @return 0 on success, non-zero on error.
 */
int32_t hcall_save_restore_sworld_ctx(struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm = vcpu->vm;

	if (vm->sworld_control.flag.supported == 0UL) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Secure World is not supported!\n");
		return -EPERM;
	}

	/* Currently, Secure World is only running on vCPU0 */
	if (!is_vcpu_bsp(vcpu)) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"This hypercall is only allowed from vcpu0!");
		return -EPERM;
	}

	if (vm->sworld_control.flag.active != 0UL) {
		save_sworld_context(vcpu);
		vm->sworld_control.flag.ctx_saved = 1UL;
	} else {
		if (vm->sworld_control.flag.ctx_saved != 0UL) {
			restore_sworld_context(vcpu);
			vm->sworld_control.flag.ctx_saved = 0UL;
			vm->sworld_control.flag.active = 1UL;
		}
	}

	return 0;
}
