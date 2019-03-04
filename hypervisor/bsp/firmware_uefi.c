/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* this is for UEFI platform */

#include <hypervisor.h>
#include <multiboot.h>
#include <boot_context.h>
#include <firmware_uefi.h>

static struct uefi_context uefi_ctx;
static struct lapic_regs uefi_lapic_regs;

static void uefi_init(void)
{
	static bool uefi_initialized = false;
	struct multiboot_info *mbi = NULL;

	if (!uefi_initialized) {
		parse_hv_cmdline();

		mbi = (struct multiboot_info *) hpa2hva(((uint64_t)(uint32_t)boot_regs[1]));
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_DRIVES) == 0U) {
			pr_err("no multiboot drivers for uefi found");
		} else {
			memcpy_s(&uefi_ctx, sizeof(struct uefi_context), hpa2hva((uint64_t)mbi->mi_drives_addr),
					sizeof(struct uefi_context));
			save_lapic(&uefi_lapic_regs);
		}
		uefi_initialized = true;
	}
}

const struct uefi_context *get_efi_ctx(void)
{
	uefi_init();
	return &uefi_ctx;
}

const struct lapic_regs *get_efi_lapic_regs(void)
{
	uefi_init();
	return &uefi_lapic_regs;
}

static uint64_t uefi_get_ap_trampoline(void)
{
	return (uint64_t)(uefi_ctx.ap_trampoline_buf);
}

static void* uefi_get_rsdp(void)
{
	return hpa2hva((uint64_t)(uefi_ctx.rsdp));
}

static void uefi_spurious_handler(int32_t vector)
{
	if (get_cpu_id() == BOOT_CPU_ID) {
		struct acrn_vcpu *vcpu = per_cpu(vcpu, BOOT_CPU_ID);

		if (vcpu != NULL) {
			vlapic_set_intr(vcpu, vector, LAPIC_TRIG_EDGE);
		} else {
			pr_err("%s vcpu or vlapic is not ready, interrupt lost\n", __func__);
		}
	}
}

static void uefi_init_irq(void)
{
	spurious_handler = (spurious_handler_t)uefi_spurious_handler;
	/* we defer irq enabling till vlapic is ready */
}

static struct firmware_operations firmware_uefi_ops = {
	.init = uefi_init,
	.get_ap_trampoline = uefi_get_ap_trampoline,
	.get_rsdp = uefi_get_rsdp,
	.init_irq = uefi_init_irq,
};

struct firmware_operations* uefi_get_firmware_operations(void)
{
	return &firmware_uefi_ops;
}
