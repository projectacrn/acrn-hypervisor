/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <hypercall.h>
#include <errno.h>
#include <logmsg.h>


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
	int32_t ret = -EPERM;

	if ((vcpu->vm->sworld_control.flag.supported != 0UL) && (next_world_id < NR_WORLD)
		&& (vcpu->vm->sworld_control.flag.active != 0UL)) {
		switch_world(vcpu, next_world_id);
		ret = 0;
	}
	return ret;
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
	int32_t ret = -EFAULT;

	if ((vcpu->vm->sworld_control.flag.supported != 0UL)
		&& (vcpu->vm->sworld_control.flag.active == 0UL)
		&& (vcpu->arch.cur_context == NORMAL_WORLD)) {
		struct trusty_boot_param boot_param;

		if (copy_from_gpa(vcpu->vm, &boot_param, param, sizeof(boot_param)) == 0) {
			if (initialize_trusty(vcpu, &boot_param)) {
				vcpu->vm->sworld_control.flag.active = 1UL;
				ret = 0;
			}
		}
	} else {
		ret = -EPERM;
		pr_err("%s, context mismatch when initialize trusty.\n", __func__);
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
	int32_t ret = -EINVAL;

	if (is_vcpu_bsp(vcpu) && (vm->sworld_control.flag.supported != 0UL)) {
		if (vm->sworld_control.flag.active != 0UL) {
			save_sworld_context(vcpu);
			vm->sworld_control.flag.ctx_saved = 1UL;
			ret = 0;
		} else {
			if (vm->sworld_control.flag.ctx_saved != 0UL) {
				restore_sworld_context(vcpu);
				vm->sworld_control.flag.ctx_saved = 0UL;
				vm->sworld_control.flag.active = 1UL;
				ret = 0;
			}
		}
	} else {
		ret = -EPERM;
		pr_err("%s, states mismatch when save restore sworld context.\n", __func__);
	}

	return ret;
}
