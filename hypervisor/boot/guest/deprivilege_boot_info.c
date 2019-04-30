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
#include <deprivilege_boot.h>

static int32_t depri_boot_sw_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);
	struct acrn_vcpu_regs *vcpu_regs = &boot_context;
	const struct depri_boot_context *depri_boot_ctx = get_depri_boot_ctx();
	const struct lapic_regs *depri_boot_lapic_regs = get_depri_boot_lapic_regs();

	pr_dbg("Loading guest to run-time location");

	vlapic_restore(vcpu_vlapic(vcpu), depri_boot_lapic_regs);

	/* For UEFI platform, the bsp init regs come from two places:
	 * 1. saved in depri_boot: gpregs, rip
	 * 2. saved when HV started: other registers
	 * We copy the info saved in depri_boot to boot_context and
	 * init bsp with boot_context.
	 */
	memcpy_s(&(vcpu_regs->gprs), sizeof(struct acrn_gp_regs),
		&(depri_boot_ctx->vcpu_regs.gprs), sizeof(struct acrn_gp_regs));

	vcpu_regs->rip = depri_boot_ctx->vcpu_regs.rip;
	set_vcpu_regs(vcpu, vcpu_regs);

	/* defer irq enabling till vlapic is ready */
	CPU_IRQ_ENABLE();

	return ret;
}

int32_t init_depri_vboot_info(__unused struct acrn_vm *vm)
{
	vm_sw_loader = depri_boot_sw_loader;

	return 0;
}
