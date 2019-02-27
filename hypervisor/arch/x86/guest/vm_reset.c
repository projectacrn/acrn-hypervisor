/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vm_reset.h>
#include <per_cpu.h>

void shutdown_vm_from_idle(uint16_t pcpu_id)
{
	struct acrn_vm *vm = get_vm_from_vmid(per_cpu(shutdown_vm_id, pcpu_id));
	const struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);

	if (vcpu->pcpu_id == pcpu_id) {
		(void)shutdown_vm(vm);
	}
}
