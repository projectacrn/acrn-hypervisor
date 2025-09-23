/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_

#include <asm/guest/vm.h>

struct acrn_vm *get_vm_from_vmid(__unused uint16_t vm_id);

bool is_paused_vm(__unused const struct acrn_vm *vm);

bool is_poweroff_vm(__unused const struct acrn_vm *vm);

void arch_trigger_level_intr(__unused struct acrn_vm *vm,
			__unused uint32_t irq, __unused bool assert);

#endif /* VM_H_ */
