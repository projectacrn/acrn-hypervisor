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
#include <bsp_extern.h>
#include <multiboot.h>
#ifdef CONFIG_EFI_STUB
#include <acrn_efi.h>
#endif
#include <hv_debug.h>

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
	if (vcpu && vcpu->arch_vcpu.vlapic) {
		ret = vlapic_set_intr(vcpu, vector, 0);
		if (ret)
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
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	ASSERT(vm != NULL, "Incorrect argument");

	pr_dbg("Loading guest to run-time location");

	if (!is_vm0(vm))
		return load_guest(vm, vcpu);

	vlapic_restore(vcpu->arch_vcpu.vlapic, &uefi_lapic_regs);

	vcpu->entry_addr = (void *)efi_ctx->rip;
	cur_context->guest_cpu_regs.regs.rax = efi_ctx->rax;
	cur_context->guest_cpu_regs.regs.rbx = efi_ctx->rbx;
	cur_context->guest_cpu_regs.regs.rdx = efi_ctx->rcx;
	cur_context->guest_cpu_regs.regs.rcx = efi_ctx->rdx;
	cur_context->guest_cpu_regs.regs.rdi = efi_ctx->rdi;
	cur_context->guest_cpu_regs.regs.rsi = efi_ctx->rsi;
	cur_context->guest_cpu_regs.regs.rbp = efi_ctx->rbp;
	cur_context->guest_cpu_regs.regs.r8 = efi_ctx->r8;
	cur_context->guest_cpu_regs.regs.r9 = efi_ctx->r9;
	cur_context->guest_cpu_regs.regs.r10 = efi_ctx->r10;
	cur_context->guest_cpu_regs.regs.r11 = efi_ctx->r11;
	cur_context->guest_cpu_regs.regs.r12 = efi_ctx->r12;
	cur_context->guest_cpu_regs.regs.r13 = efi_ctx->r13;
	cur_context->guest_cpu_regs.regs.r14 = efi_ctx->r14;
	cur_context->guest_cpu_regs.regs.r15 = efi_ctx->r15;

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
