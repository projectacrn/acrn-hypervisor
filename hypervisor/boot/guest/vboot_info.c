/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <x86/per_cpu.h>
#include <x86/irq.h>
#include <multiboot.h>
#include <x86/pgtable.h>
#include <x86/zeropage.h>
#include <x86/seed.h>
#include <x86/mmu.h>
#include <x86/guest/vm.h>
#include <logmsg.h>
#include <vboot_info.h>
#include <vacpi.h>

#define DBG_LEVEL_BOOT	6U

/**
 * @pre vm != NULL && mbi != NULL
 */
static void init_vm_ramdisk_info(struct acrn_vm *vm, const struct multiboot_module *mod)
{
	void *mod_addr = hpa2hva((uint64_t)mod->mm_mod_start);

	if ((mod_addr != NULL) && (mod->mm_mod_end > mod->mm_mod_start)) {
		vm->sw.ramdisk_info.src_addr = mod_addr;
		vm->sw.ramdisk_info.load_addr = vm->sw.kernel_info.kernel_load_addr + vm->sw.kernel_info.kernel_size;
		vm->sw.ramdisk_info.load_addr = (void *)round_page_up((uint64_t)vm->sw.ramdisk_info.load_addr);
		vm->sw.ramdisk_info.size = mod->mm_mod_end - mod->mm_mod_start;
	}
}

/**
 * @pre vm != NULL && mod != NULL
 */
static void init_vm_acpi_info(struct acrn_vm *vm, const struct multiboot_module *mod)
{
	vm->sw.acpi_info.src_addr = hpa2hva((uint64_t)mod->mm_mod_start);
	vm->sw.acpi_info.load_addr = (void *)VIRT_ACPI_DATA_ADDR;
	vm->sw.acpi_info.size = ACPI_MODULE_SIZE;
}

/**
 * @pre vm != NULL
 */
static void *get_kernel_load_addr(struct acrn_vm *vm)
{
	void *load_addr = NULL;
	struct vm_sw_info *sw_info = &vm->sw;
	struct zero_page *zeropage;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	switch (sw_info->kernel_type) {
	case KERNEL_BZIMAGE:
		/* According to the explaination for pref_address
		 * in Documentation/x86/boot.txt, a relocating
		 * bootloader should attempt to load kernel at pref_address
		 * if possible. A non-relocatable kernel will unconditionally
		 * move itself and to run at this address, so no need to copy
		 * kernel to perf_address by bootloader, if kernel is
		 * non-relocatable.
		 */
		zeropage = (struct zero_page *)sw_info->kernel_info.kernel_src_addr;
		if (zeropage->hdr.relocatable_kernel != 0U) {
			zeropage = (struct zero_page *)zeropage->hdr.pref_addr;
		}
		load_addr = (void *)zeropage;
		break;
	case KERNEL_ZEPHYR:
		load_addr = (void *)vm_config->os_config.kernel_load_addr;
		break;
	default:
		pr_err("Unsupported Kernel type.");
		break;
	}
	if (load_addr == NULL) {
		pr_err("Could not get kernel load addr of VM %d .", vm->vm_id);
	}
	return load_addr;
}

/**
 * @pre vm != NULL && mod != NULL
 */
static int32_t init_vm_kernel_info(struct acrn_vm *vm, const struct multiboot_module *mod)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	dev_dbg(DBG_LEVEL_BOOT, "kernel mod start=0x%x, end=0x%x",
		mod->mm_mod_start, mod->mm_mod_end);

	vm->sw.kernel_type = vm_config->os_config.kernel_type;
	vm->sw.kernel_info.kernel_src_addr = hpa2hva((uint64_t)mod->mm_mod_start);
	if ((vm->sw.kernel_info.kernel_src_addr != NULL) && (mod->mm_mod_end > mod->mm_mod_start)){
		vm->sw.kernel_info.kernel_size = mod->mm_mod_end - mod->mm_mod_start;
		vm->sw.kernel_info.kernel_load_addr = get_kernel_load_addr(vm);
	}

	return (vm->sw.kernel_info.kernel_load_addr == NULL) ? (-EINVAL) : 0;
}

/* cmdline parsed from multiboot module string, for pre-launched VMs and SOS VM only. */
static char mod_cmdline[PRE_VM_NUM + SOS_VM_NUM][MAX_BOOTARGS_SIZE] = { '\0' };

/**
 * @pre vm != NULL && mbi != NULL
 */
static void init_vm_bootargs_info(struct acrn_vm *vm, const struct acrn_multiboot_info *mbi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	char *bootargs = vm_config->os_config.bootargs;

	if ((vm_config->load_order == PRE_LAUNCHED_VM) || (vm_config->load_order == SOS_VM)) {
		if (mod_cmdline[vm->vm_id][0] == '\0') {
			vm->sw.bootargs_info.src_addr = bootargs;
		} else {
			/* override build-in bootargs with multiboot module string which is configurable
			 * at bootloader boot time. e.g. GRUB menu
			 */
			vm->sw.bootargs_info.src_addr = &mod_cmdline[vm->vm_id][0];
		}
	}

	if (vm_config->load_order == SOS_VM) {
		if (strncat_s((char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE, " ", 1U) == 0) {
			char seed_args[MAX_SEED_ARG_SIZE] = "";

			fill_seed_arg(seed_args, true);
			/* Fill seed argument for SOS
			 * seed_args string ends with a white space and '\0', so no aditional delimiter is needed
			 */
			if (strncat_s((char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE,
					seed_args, (MAX_BOOTARGS_SIZE - 1U)) != 0) {
				pr_err("failed to fill seed arg to SOS bootargs!");
			}

			/* If there is cmdline from mbi->mi_cmdline, merge it with configured SOS bootargs. */
			if (((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) && (*(mbi->mi_cmdline) != '\0')) {
				if (strncat_s((char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE,
						mbi->mi_cmdline, (MAX_BOOTARGS_SIZE - 1U)) != 0) {
					pr_err("failed to merge mbi cmdline to SOS bootargs!");
				}
			}
		} else {
			pr_err("no space to append SOS bootargs!");
		}

	}

	vm->sw.bootargs_info.size = strnlen_s((const char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE);

	/* Kernel bootarg and zero page are right before the kernel image */
	if (vm->sw.bootargs_info.size > 0U) {
		vm->sw.bootargs_info.load_addr = vm->sw.kernel_info.kernel_load_addr - (MEM_1K * 8U);
	} else {
		vm->sw.bootargs_info.load_addr = NULL;
	}
}

/* @pre mbi != NULL && tag != NULL
 */
static struct multiboot_module *get_mod_by_tag(const struct acrn_multiboot_info *mbi, const char *tag)
{
	uint8_t i;
	struct multiboot_module *mod = NULL;
	struct multiboot_module *mods = (struct multiboot_module *)(&mbi->mi_mods[0]);
	uint32_t tag_len = strnlen_s(tag, MAX_MOD_TAG_LEN);

	for (i = 0U; i < mbi->mi_mods_count; i++) {
		const char *mm_string = (char *)hpa2hva((uint64_t)(mods + i)->mm_string);
		uint32_t mm_str_len = strnlen_s(mm_string, MAX_MOD_TAG_LEN);
		const char *p_chr = mm_string + tag_len; /* point to right after the end of tag */

		/* The tag must be located at the first word in mm_string and end with SPACE/TAB or EOL since
		 * when do file stitch by tool, the tag in mm_string might be followed by EOL(0x0d/0x0a).
		 */
		if ((mm_str_len >= tag_len) && (strncmp(mm_string, tag, tag_len) == 0)
				&& (is_space(*p_chr) || is_eol(*p_chr))) {
			mod = mods + i;
			break;
		}
	}
	/* GRUB might put module at address 0 or under 1MB in the case that the module size is less then 1MB
	 * ACRN will not support these cases
	 */
	if ((mod != NULL) && ((mod->mm_mod_start == 0U) || (mod->mm_mod_end <= MEM_1M))) {
		pr_err("Unsupported multiboot module: start at 0x%x, end at 0x%x", mod->mm_mod_start, mod->mm_mod_end);
		mod = NULL;
	}

	return mod;
}

/* @pre vm != NULL && mbi != NULL
 */
static int32_t init_vm_sw_load(struct acrn_vm *vm, const struct acrn_multiboot_info *mbi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct multiboot_module *mod;
	int32_t ret = -EINVAL;

	dev_dbg(DBG_LEVEL_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

	/* find kernel module first */
	mod = get_mod_by_tag(mbi, vm_config->os_config.kernel_mod_tag);
	if (mod != NULL) {
		const char *mm_string = (char *)hpa2hva((uint64_t)mod->mm_string);
		uint32_t mm_str_len = strnlen_s(mm_string, MAX_BOOTARGS_SIZE);
		uint32_t tag_len = strnlen_s(vm_config->os_config.kernel_mod_tag, MAX_MOD_TAG_LEN);
		const char *p_chr = mm_string + tag_len + 1; /* point to the possible start of cmdline */

		/* check whether there is a cmdline configured in module string */
		if (((mm_str_len > (tag_len + 1U))) && (is_space(*(p_chr - 1))) && (!is_eol(*p_chr))) {
			(void)strncpy_s(&mod_cmdline[vm->vm_id][0], MAX_BOOTARGS_SIZE,
					p_chr, (MAX_BOOTARGS_SIZE - 1U));
		}

		ret = init_vm_kernel_info(vm, mod);
	}

	if (ret == 0) {
		/* Currently VM bootargs only support Linux guest */
		if (vm->sw.kernel_type == KERNEL_BZIMAGE) {
			init_vm_bootargs_info(vm, mbi);
		}
		/* check whether there is a ramdisk module */
		mod = get_mod_by_tag(mbi, vm_config->os_config.ramdisk_mod_tag);
		if (mod != NULL) {
			init_vm_ramdisk_info(vm, mod);
		}

		if (is_prelaunched_vm(vm)) {
			mod = get_mod_by_tag(mbi, vm_config->acpi_config.acpi_mod_tag);
			if ((mod != NULL) && ((mod->mm_mod_end - mod->mm_mod_start) == ACPI_MODULE_SIZE)) {
				init_vm_acpi_info(vm, mod);
			} else {
				pr_err("failed to load VM %d acpi module", vm->vm_id);
			}
		}

	} else {
		pr_err("failed to load VM %d kernel module", vm->vm_id);
	}
	return ret;
}

/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @retval 0 on success
 * @retval -EINVAL on invalid parameters
 *
 * @pre vm != NULL
 */
int32_t init_vm_boot_info(struct acrn_vm *vm)
{
	struct acrn_multiboot_info *mbi = get_acrn_multiboot_info();
	int32_t ret = -EINVAL;

	stac();
	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS) == 0U) {
		panic("no multiboot module info found");
	} else {
		ret = init_vm_sw_load(vm, mbi);
	}
	clac();

	return ret;
}
