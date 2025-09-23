/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VM_C_
#define RISCV_VM_C_


#include <asm/guest/vm.h>

/* FIXME */
struct acrn_vm *get_vm_from_vmid(__unused uint16_t vm_id)
{
	return NULL;
}

bool is_paused_vm(__unused const struct acrn_vm *vm)
{
	return false;
}

bool is_poweroff_vm(__unused const struct acrn_vm *vm)
{
	return true;
}

void arch_trigger_level_intr(__unused struct acrn_vm *vm,
			__unused uint32_t irq, __unused bool assert) {}

#endif /* RISCV_VM_C_ */
