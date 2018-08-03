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
int32_t hcall_world_switch(struct vcpu *vcpu)
{
	int32_t next_world_id = !(vcpu->arch_vcpu.cur_context);

	if (next_world_id >= NR_WORLD) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"%s world_id %d exceed max number of Worlds\n",
			__func__, next_world_id);
		return -EINVAL;
	}

	if (!vcpu->vm->sworld_control.flag.supported) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Secure World is not supported!\n");
		return -EPERM;
	}

	if (!vcpu->vm->sworld_control.flag.active) {
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
int32_t hcall_initialize_trusty(struct vcpu *vcpu, uint64_t param)
{
	if (!vcpu->vm->sworld_control.flag.supported) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Secure World is not supported!\n");
		return -EPERM;
	}

	if (vcpu->vm->sworld_control.flag.active) {
		dev_dbg(ACRN_DBG_TRUSTY_HYCALL,
			"Trusty already initialized!\n");
		return -EPERM;
	}

	if (vcpu->arch_vcpu.cur_context != NORMAL_WORLD) {
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
