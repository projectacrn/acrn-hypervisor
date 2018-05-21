/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hypercall.h>
#include <acrn_hv_defs.h>
#include <hv_debug.h>

int64_t hcall_world_switch(struct vcpu *vcpu)
{
	int next_world_id = !(vcpu->arch_vcpu.cur_context);

	if (!vcpu->vm->sworld_control.sworld_enabled) {
		pr_err("Secure World is not enabled!\n");
		return -1;
	}

	if (!vcpu->vm->arch_vm.sworld_eptp) {
		pr_err("Trusty is not initialized!\n");
		return -1;
	}

	ASSERT(next_world_id < NR_WORLD,
		"world_id exceed max number of Worlds");

	switch_world(vcpu, next_world_id);
	return 0;
}

int64_t hcall_initialize_trusty(struct vcpu *vcpu, uint64_t param)
{
	if (!vcpu->vm->sworld_control.sworld_enabled) {
		pr_err("Secure World is not enabled!\n");
		return -1;
	}

	if (vcpu->vm->arch_vm.sworld_eptp) {
		pr_err("Trusty already initialized!\n");
		return -1;
	}

	ASSERT(vcpu->arch_vcpu.cur_context == NORMAL_WORLD,
		"The Trusty Initialize hypercall must be from Normal World");

	if (!initialize_trusty(vcpu, param)) {
		pr_err("Failed to initialize trusty!\n");
		return -1;
	}

	return 0;
}
