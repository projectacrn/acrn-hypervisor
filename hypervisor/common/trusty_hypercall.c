/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hypercall.h>

#define ACRN_DBG_TRUSTY_HYCALL 6U

/* this hcall is only come from trusty enabled vcpu itself, and cannot be
 * called from other vcpus
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

/* this hcall is only come from trusty enabled vcpu itself, and cannot be
 * called from other vcpus
 */
int32_t hcall_initialize_trusty(struct acrn_vcpu *vcpu, uint64_t param)
{
	if (vcpu->vm->sworld_control.flag.supported == 0UL) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Secure World is not supported!\n");
		return -EPERM;
	}

	if (vcpu->vm->sworld_control.flag.active != 0UL) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Trusty already initialized!\n");
		return -EPERM;
	}

	if (vcpu->arch.cur_context != NORMAL_WORLD) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"%s, must initialize Trusty from Normal World!\n",
			__func__);
		return -EPERM;
	}

	if (!initialize_trusty(vcpu, param)) {
		return -ENODEV;
	}

	vcpu->vm->sworld_control.flag.active = 1UL;

	return 0;
}

int64_t hcall_save_restore_sworld_ctx(struct acrn_vcpu *vcpu)
{
	struct vm *vm = vcpu->vm;

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
