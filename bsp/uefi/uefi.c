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
uint32_t efi_physical_available_ap_bitmap = 0;
uint32_t efi_wake_up_ap_bitmap = 0;
struct efi_ctx* efi_ctx = NULL;
extern uint32_t up_count;
extern unsigned long pcpu_sync;

void efi_spurious_handler(int vector)
{
	return;
}

int sipi_from_efi_boot_service_exit(uint32_t dest, uint32_t mode, uint32_t vec)
{
	if (efi_wake_up_ap_bitmap != efi_physical_available_ap_bitmap) {
		if (mode == APIC_DELMODE_STARTUP) {
			uint32_t cpu_id = cpu_find_logical_id(dest);
			send_startup_ipi(INTR_CPU_STARTUP_USE_DEST,
		       cpu_id, (paddr_t)(vec<<12));
			efi_wake_up_ap_bitmap |= 1 << dest;
		}

		return 1;
	}

	return 0;
}

void efi_deferred_wakeup_pcpu(int cpu_id)
{
	uint32_t timeout;
	uint32_t expected_up;

	expected_up = up_count + 1;

	send_startup_ipi(INTR_CPU_STARTUP_USE_DEST,
		cpu_id, (paddr_t)cpu_secondary_reset);

	timeout = CPU_UP_TIMEOUT * 1000;

	while ((up_count != expected_up)) {
		/* Delay 10us */
		udelay(10);

		/* Decrement timeout value */
		timeout -= 10;
	}

	bitmap_set(0, &pcpu_sync);
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

	vcpu->entry_addr = efi_ctx->entry;
	cur_context->guest_cpu_regs.regs.rcx = efi_ctx->handle;
	cur_context->guest_cpu_regs.regs.rdx = efi_ctx->table;

	return ret;
}
#endif

void init_bsp(void)
{
	parse_hv_cmdline();

#ifdef CONFIG_EFI_STUB
	efi_ctx = (struct efi_ctx*)(uint64_t)boot_regs[2];
	ASSERT(efi_ctx != NULL, "");

	vm_sw_loader = uefi_sw_loader;

	spurious_handler = efi_spurious_handler;
#endif
}
