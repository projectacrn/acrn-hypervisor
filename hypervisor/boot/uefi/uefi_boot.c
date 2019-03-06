/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <cpu.h>
#include <per_cpu.h>
#include <guest/vm.h>
#include <boot_context.h>
#include <firmware_uefi.h>

static int32_t uefi_sw_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);
	struct acrn_vcpu_regs *vcpu_regs = &boot_context;
	const struct uefi_context *uefi_ctx = get_uefi_ctx();
	const struct lapic_regs *uefi_lapic_regs = get_uefi_lapic_regs();

	pr_dbg("Loading guest to run-time location");

	vlapic_restore(vcpu_vlapic(vcpu), uefi_lapic_regs);

	/* For UEFI platform, the bsp init regs come from two places:
	 * 1. saved in uefi_boot: gpregs, rip
	 * 2. saved when HV started: other registers
	 * We copy the info saved in uefi_boot to boot_context and
	 * init bsp with boot_context.
	 */
	memcpy_s(&(vcpu_regs->gprs), sizeof(struct acrn_gp_regs),
		&(uefi_ctx->vcpu_regs.gprs), sizeof(struct acrn_gp_regs));

	vcpu_regs->rip = uefi_ctx->vcpu_regs.rip;
	set_vcpu_regs(vcpu, vcpu_regs);

	/* defer irq enabling till vlapic is ready */
	CPU_IRQ_ENABLE();

	return ret;
}

int32_t uefi_init_vm_boot_info(__unused struct acrn_vm *vm)
{
	vm_sw_loader = uefi_sw_loader;

	return 0;
}
