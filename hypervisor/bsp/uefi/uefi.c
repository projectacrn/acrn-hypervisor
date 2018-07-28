/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <multiboot.h>
#ifdef CONFIG_EFI_STUB
#include <vm0_boot.h>
#endif

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

struct efi_ctx* efi_ctx = NULL;
struct lapic_regs uefi_lapic_regs;
static int efi_initialized;

void efi_spurious_handler(int vector)
{
	struct vcpu* vcpu;
	int ret;

	if (get_cpu_id() != 0)
		return;

	vcpu = per_cpu(vcpu, 0);
	if ((vcpu != NULL) && vcpu->arch_vcpu.vlapic) {
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

	ASSERT(vm != NULL, "Incorrect argument");

	pr_dbg("Loading guest to run-time location");

	if (!is_vm0(vm))
		return load_guest(vm, vcpu);

	vlapic_restore(vcpu->arch_vcpu.vlapic, &uefi_lapic_regs);

	vcpu->entry_addr = (void *)efi_ctx->rip;
	vcpu_set_gpreg(vcpu, CPU_REG_RAX, efi_ctx->rax);
	vcpu_set_gpreg(vcpu, CPU_REG_RBX, efi_ctx->rbx);
	vcpu_set_gpreg(vcpu, CPU_REG_RCX, efi_ctx->rcx);
	vcpu_set_gpreg(vcpu, CPU_REG_RDX, efi_ctx->rdx);
	vcpu_set_gpreg(vcpu, CPU_REG_RDI, efi_ctx->rdi);
	vcpu_set_gpreg(vcpu, CPU_REG_RSI, efi_ctx->rsi);
	vcpu_set_gpreg(vcpu, CPU_REG_RBP, efi_ctx->rbp);
	vcpu_set_gpreg(vcpu, CPU_REG_R8, efi_ctx->r8);
	vcpu_set_gpreg(vcpu, CPU_REG_R9, efi_ctx->r9);
	vcpu_set_gpreg(vcpu, CPU_REG_R10, efi_ctx->r10);
	vcpu_set_gpreg(vcpu, CPU_REG_R11, efi_ctx->r11);
	vcpu_set_gpreg(vcpu, CPU_REG_R12, efi_ctx->r12);
	vcpu_set_gpreg(vcpu, CPU_REG_R13, efi_ctx->r13);
	vcpu_set_gpreg(vcpu, CPU_REG_R14, efi_ctx->r14);
	vcpu_set_gpreg(vcpu, CPU_REG_R15, efi_ctx->r15);

	/* defer irq enabling till vlapic is ready */
	CPU_IRQ_ENABLE();

	return ret;
}

void *get_rsdp_from_uefi(void)
{
	if (!efi_initialized)
		efi_init();

	return HPA2HVA(efi_ctx->rsdp);
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

	mbi = (struct multiboot_info *)HPA2HVA(((uint64_t)(uint32_t)boot_regs[1]));

	if (!(mbi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES))
		ASSERT(0, "no multiboot drivers for uefi found");

	efi_ctx = (struct efi_ctx *)HPA2HVA((uint64_t)mbi->mi_drives_addr);
	ASSERT(efi_ctx != NULL, "no uefi context found");

	vm_sw_loader = uefi_sw_loader;

	spurious_handler = efi_spurious_handler;

	save_lapic(&uefi_lapic_regs);

	efi_initialized = 1;
}
#endif

void init_bsp(void)
{
	parse_hv_cmdline();

#ifdef CONFIG_EFI_STUB
	if (!efi_initialized)
		efi_init();
#endif
}
