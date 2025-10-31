/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Yifan Liu <yifan1.liu@intel.com>
 *         Haicheng Li <haicheng.li@intel.com>
 */

#include <vm.h>
#include <vcpu.h>
#include <fdt.h>
#include <vcpu.h>
#include <reloc.h>
#include <cpu.h>
#include <per_cpu.h>

#include <asm/guest/vcpu_priv.h>

uint32_t vcpu_get_vhartid(struct acrn_vcpu *vcpu)
{
	uint32_t vhartid = vcpu->vcpu_id;

	if (is_service_vm(vcpu->vm)) {
		/* vhartid == phartid */
		vhartid = per_cpu(arch.hart_id, pcpuid_from_vcpu(vcpu));
	}

	/* RISC-V requires that at least one hart has hart ID 0,
	 * so we use logical ID (vcpu_id) for non-service vms.
	 */

	return vhartid;
}

struct acrn_vcpu *vcpu_from_vhartid(struct acrn_vm *vm, uint32_t vhartid)
{
	struct acrn_vcpu *vcpu;

	if (is_service_vm(vm)) {
		/* vhartid == phartid */
		vcpu = vcpu_from_pid(vm, get_pcpu_id_from_hart_id(vhartid));
	} else {
		/* vhartid == vcpu_id */
		vcpu = vcpu_from_vid(vm, vhartid);
	}
	return vcpu;
}

int32_t arch_init_vm(struct acrn_vm *vm, struct acrn_vm_config *vm_config)
{
	(void)vm;
	(void)vm_config;
	return 0;
}

int32_t arch_deinit_vm(struct acrn_vm *vm)
{
	(void)vm;
	return 0;
}

int32_t arch_reset_vm(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		reset_vcpu(vcpu);
	}
	return 0;
}

void arch_vm_prepare_bsp(struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm = vcpu->vm;

	vcpu_set_epc(vcpu, (uint64_t)vm->sw.kernel_info.kernel_entry_addr);

	vcpu->arch.regs.a0 = vcpu_get_vhartid(vcpu);
	vcpu->arch.regs.a1 = (uint64_t)vm->sw.fdt_info.load_addr;
}

void arch_trigger_level_intr(__unused struct acrn_vm *vm,
			__unused uint32_t irq, __unused bool assert) {}
