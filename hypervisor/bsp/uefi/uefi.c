/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <multiboot.h>
#include <vm0_boot.h>

/* IOAPIC id */
#define UEFI_IOAPIC_ID   8
/* IOAPIC base address */
#define UEFI_IOAPIC_ADDR 0xfec00000
/* IOAPIC range size */
#define UEFI_IOAPIC_SIZE 0x100000
/* Local APIC base address */
#define UEFI_LAPIC_ADDR 0xfee00000
/* Local APIC range size */
#define UEFI_LAPIC_SIZE 0x100000
/* Number of PCI IRQ assignments */
#define UEFI_PCI_IRQ_ASSIGNMENT_NUM 28

#ifdef CONFIG_EFI_STUB
static void efi_init(void);

struct boot_ctx* efi_ctx = NULL;
struct lapic_regs uefi_lapic_regs;
static int efi_initialized;

void efi_spurious_handler(int vector)
{
	struct vcpu* vcpu;
	int ret;

	if (get_cpu_id() != 0)
		return;

	vcpu = per_cpu(vcpu, 0);
	if (vcpu != NULL) {
		ret = vlapic_set_intr(vcpu, vector, 0);
		if (ret != 0)
			pr_err("%s vlapic set intr fail, interrupt lost\n",
				__func__);
	} else
		pr_err("%s vcpu or vlapic is not ready, interrupt lost\n",
			__func__);

	return;
}

int uefi_sw_loader(struct vm *vm, struct vcpu *vcpu)
{
	int ret = 0;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].run_ctx;

	ASSERT(vm != NULL, "Incorrect argument");

	pr_dbg("Loading guest to run-time location");

	if (!is_vm0(vm))
		return load_guest(vm, vcpu);

	vlapic_restore(vcpu_vlapic(vcpu), &uefi_lapic_regs);

	vcpu->entry_addr = (void *)efi_ctx->rip;
	memcpy_s(&cur_context->guest_cpu_regs, sizeof(struct cpu_gp_regs),
		 &efi_ctx->gprs, sizeof(struct cpu_gp_regs));

	/* defer irq enabling till vlapic is ready */
	CPU_IRQ_ENABLE();

	return ret;
}

void *get_rsdp_from_uefi(void)
{
	if (!efi_initialized)
		efi_init();

	return hpa2hva(efi_ctx->rsdp);
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

	efi_ctx = (struct boot_ctx *)hpa2hva((uint64_t)mbi->mi_drives_addr);
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
