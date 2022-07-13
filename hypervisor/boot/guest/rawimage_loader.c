/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <vboot.h>

/**
 * @pre vm != NULL
 */
static void load_rawimage(struct acrn_vm *vm)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	const struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	uint64_t kernel_load_gpa;

	/* TODO: GPA 0 load support */
	kernel_load_gpa = vm_config->os_config.kernel_load_addr;

	/* Copy the guest kernel image to its run-time location */
	(void)copy_to_gpa(vm, sw_kernel->kernel_src_addr, kernel_load_gpa, sw_kernel->kernel_size);

	sw_kernel->kernel_entry_addr = (void *)vm_config->os_config.kernel_entry_addr;
}

int32_t rawimage_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	uint64_t vgdt_gpa = 0x800;

	/*
	 * TODO:
	 *    - We need to initialize the guest BSP(boot strap processor) registers according to
	 *	guest boot mode (real mode vs protect mode)
	 *    - The memory layout usage is unclear, only GDT might be needed as its boot param.
	 *	currently we only support Zephyr which has no needs on cmdline/e820/efimmap/etc.
	 *	hardcode the vGDT GPA to 0x800 where is not used by Zephyr so far;
	 */
	init_vcpu_protect_mode_regs(vcpu_from_vid(vm, BSP_CPU_ID), vgdt_gpa);

	load_rawimage(vm);

	return ret;
}
