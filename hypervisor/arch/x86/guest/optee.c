/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/guest/ept.h>
#include <asm/vm_config.h>
#include <asm/mmu.h>
#include <asm/guest/optee.h>
#include <asm/trampoline.h>
#include <reloc.h>
#include <hypercall.h>

void prepare_tee_vm_memmap(struct acrn_vm *vm, const struct acrn_vm_config *vm_config)
{
	uint64_t hv_hpa;

	/*
	 * Only need to map following things, let init_vpci to map the secure devices
	 * if any.
	 *
	 * 1. go through physical e820 table, to ept add all system memory entries.
	 * 2. remove hv owned memory.
	 */
	if ((vm_config->guest_flags & GUEST_FLAG_TEE) != 0U) {
		vm->e820_entry_num = get_e820_entries_count();
		vm->e820_entries = (struct e820_entry *)get_e820_entry();

		prepare_vm_identical_memmap(vm, E820_TYPE_RAM, EPT_WB | EPT_RWX);

		hv_hpa = hva2hpa((void *)(get_hv_image_base()));
		ept_del_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, hv_hpa, get_hv_ram_size());
	}
}

int32_t hcall_handle_tee_vcpu_boot_done(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return 0;
}

int32_t hcall_switch_ee(struct acrn_vcpu *vcpu, __unused struct acrn_vm *target_vm,
		__unused uint64_t param1, __unused uint64_t param2)
{
	return 0;
}
