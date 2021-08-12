/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <vboot.h>
#include <elf.h>
#include <logmsg.h>

/**
 * @pre vm != NULL
 * must run in stac/clac context
 */
static void *do_load_elf64(struct acrn_vm *vm)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	void *p_elf_img = (void *)sw_kernel->kernel_src_addr;

	return p_elf_img;
}

/**
 * @pre vm != NULL
 * must run in stac/clac context
 */
static void *do_load_elf32(struct acrn_vm *vm)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	void *p_elf_img = (void *)sw_kernel->kernel_src_addr;

	return p_elf_img;
}

/**
 * @pre vm != NULL
 */
static int32_t load_elf(struct acrn_vm *vm)
{
	void *elf_entry = NULL;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	void *p_elf_img = (void *)sw_kernel->kernel_src_addr;
	int32_t ret = 0;

	stac();

	if (*(uint32_t *)p_elf_img == ELFMAGIC) {
		if (*(uint8_t *)(p_elf_img + EI_CLASS) == ELFCLASS64) {
			elf_entry = do_load_elf64(vm);
		} else if (*(uint8_t *)(p_elf_img + EI_CLASS) == ELFCLASS32) {
			elf_entry = do_load_elf32(vm);
		} else {
			pr_err("%s, unsupported elf class(%d)", __func__, *(uint8_t *)(p_elf_img + EI_CLASS));
		}
	} else {
		pr_err("%s, booting elf but no elf header found!", __func__);
	}

	clac();

	sw_kernel->kernel_entry_addr = elf_entry;

	if (elf_entry == NULL) {
		ret = -EFAULT;
	}

	return ret;
}

int32_t elf_loader(struct acrn_vm *vm)
{
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

	return load_elf(vm);
}
