/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hypercall.h>

/* this hcall is only come from trusty enabled vcpu itself, and cannot be
 * called from other vcpus
 */
int64_t hcall_world_switch(struct vcpu *vcpu)
{
	int next_world_id = !(vcpu->arch_vcpu.cur_context);

	if (next_world_id >= NR_WORLD) {
		pr_err("%s world_id %d exceed max number of Worlds\n",
			__func__, next_world_id);
		return -EINVAL;
	}

	if (!vcpu->vm->sworld_control.sworld_enabled) {
		pr_err("%s, Secure World is not enabled!\n", __func__);
		return -EPERM;
	}

	if (!vcpu->vm->arch_vm.sworld_eptp) {
		pr_err("%s, Trusty is not initialized!\n", __func__);
		return -EPERM;
	}

	switch_world(vcpu, next_world_id);
	return 0;
}

/* this hcall is only come from trusty enabled vcpu itself, and cannot be
 * called from other vcpus
 */
int64_t hcall_initialize_trusty(struct vcpu *vcpu, uint64_t param)
{
	if (!vcpu->vm->sworld_control.sworld_enabled) {
		pr_err("%s, Secure World is not enabled!\n", __func__);
		return -EPERM;
	}

	if (vcpu->vm->arch_vm.sworld_eptp) {
		pr_err("%s, Trusty already initialized!\n", __func__);
		return -EPERM;
	}

	if (vcpu->arch_vcpu.cur_context != NORMAL_WORLD) {
		pr_err("%s, must initialize Trusty from Normal World!\n",
			__func__);
		return -EPERM;
	}

	if (!initialize_trusty(vcpu, param))
		return -ENODEV;

	return 0;
}
