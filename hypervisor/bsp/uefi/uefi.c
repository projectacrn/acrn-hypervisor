/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <multiboot.h>
#include <boot_context.h>
#include <vm0_boot.h>

#ifdef CONFIG_EFI_STUB
static void efi_init(void);

struct efi_context* efi_ctx = NULL;
struct lapic_regs uefi_lapic_regs;
static int efi_initialized;

void efi_spurious_handler(int vector)
{
	struct acrn_vcpu* vcpu;

	if (get_cpu_id() != 0)
		return;

	vcpu = per_cpu(vcpu, 0);
	if (vcpu != NULL) {
		vlapic_set_intr(vcpu, vector, 0);
	} else
		pr_err("%s vcpu or vlapic is not ready, interrupt lost\n",
			__func__);

	return;
}

int uefi_sw_loader(struct acrn_vm *vm)
{
	int ret = 0;
	struct acrn_vcpu *vcpu = get_primary_vcpu(vm);
	struct acrn_vcpu_regs *vcpu_regs = &boot_context;

	ASSERT(vm != NULL, "Incorrect argument");

	pr_dbg("Loading guest to run-time location");

	vlapic_restore(vcpu_vlapic(vcpu), &uefi_lapic_regs);

	/* For UEFI platform, the bsp init regs come from two places:
	 * 1. saved in efi_boot: gpregs, rip
	 * 2. saved when HV started: other registers
	 * We copy the info saved in efi_boot to boot_context and
	 * init bsp with boot_context.
	 */
	memcpy_s(&(vcpu_regs->gprs), sizeof(struct acrn_gp_regs),
		&(efi_ctx->vcpu_regs.gprs), sizeof(struct acrn_gp_regs));

	vcpu_regs->rip = efi_ctx->vcpu_regs.rip;
	set_vcpu_regs(vcpu, vcpu_regs);

	/* defer irq enabling till vlapic is ready */
	CPU_IRQ_ENABLE();

	return ret;
}

void *get_rsdp_from_uefi(void)
{
	if (!efi_initialized)
		efi_init();

	return hpa2hva((uint64_t)efi_ctx->rsdp);
}

void *get_ap_trampoline_buf(void)
{
	return efi_ctx->ap_trampoline_buf;
}

static void efi_init(void)
{
	struct multiboot_info *mbi = NULL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC)
		ASSERT(0, "no multiboot info found");

	mbi = (struct multiboot_info *)
				hpa2hva(((uint64_t)(uint32_t)boot_regs[1]));

	if (!(mbi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES))
		ASSERT(0, "no multiboot drivers for uefi found");

	efi_ctx = (struct efi_context *)hpa2hva((uint64_t)mbi->mi_drives_addr);
	ASSERT(efi_ctx != NULL, "no uefi context found");

	vm_sw_loader = uefi_sw_loader;

	spurious_handler = (spurious_handler_t)efi_spurious_handler;

	save_lapic(&uefi_lapic_regs);

	efi_initialized = 1;
}
#endif

void init_bsp(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
	parse_hv_cmdline();

#ifdef CONFIG_EFI_STUB
	if (!efi_initialized)
		efi_init();
#endif
}
