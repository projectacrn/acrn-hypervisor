/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <vcpu.h>
#include <vcpu.h>

int32_t arch_init_vm(struct acrn_vm *vm, struct acrn_vm_config *vm_config)
{
	(void)vm;
	(void)vm_config;
	return 0;
}

int32_t arch_deinit_vm(struct acrn_vm *vm)
{
	(void)vm;
	return 0;
}

int32_t arch_reset_vm(struct acrn_vm *vm)
{
	(void)vm;
	return 0;
}

void arch_vm_prepare_bsp(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
}

void arch_trigger_level_intr(__unused struct acrn_vm *vm,
			__unused uint32_t irq, __unused bool assert) {}
