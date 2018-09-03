/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <multiboot.h>
#include <zeropage.h>
#include <hob_parse.h>

#define BOOT_ARGS_LOAD_ADDR				0x24EFC000

#define ACRN_DBG_BOOT	6U

#define MAX_BOOT_PARAMS_LEN 64U

static const char *boot_params_arg = "ImageBootParamsAddr=";

struct image_boot_params {
	uint32_t size_of_this_struct;
	uint32_t version;
	uint64_t p_seed_list;
	uint64_t p_platform_info;
	uint64_t reserved;
};

/* There are two sources for vm0 kernel cmdline:
 * - cmdline from sbl. mbi->cmdline
 * - cmdline from acrn stitching tool. mod[0].mm_string
 * We need to merge them together
 */
static char kernel_cmdline[MEM_2K];

/* now modules support: FIRMWARE & RAMDISK & SeedList */
static void parse_other_modules(struct vm *vm,
	struct multiboot_module *mods, uint32_t mods_count)
{
	uint32_t i;

	for (i = 0U; i < mods_count; i++) {
		uint32_t type_len;
		const char *start = HPA2HVA((uint64_t)mods[i].mm_string);
		const char *end;
		void *mod_addr = HPA2HVA((uint64_t)mods[i].mm_mod_start);
		uint32_t mod_size = mods[i].mm_mod_end - mods[i].mm_mod_start;

		dev_dbg(ACRN_DBG_BOOT, "other mod-%d start=0x%x, end=0x%x",
			i, mods[i].mm_mod_start, mods[i].mm_mod_end);
		dev_dbg(ACRN_DBG_BOOT, "cmd addr=0x%x, str=%s",
			mods[i].mm_string, start);

		while (*start == ' ') {
			start++;
		}

		end = start;
		while (((*end) != ' ') && ((*end) != '\0')) {
			end++;
		}

		type_len = end - start;
		if (strncmp("FIRMWARE", start, type_len) == 0) {
			char  dyn_bootargs[100] = {'\0'};
			void *load_addr = GPA2HVA(vm,
				(uint64_t)vm->sw.linux_info.bootargs_load_addr);
			uint32_t args_size = vm->sw.linux_info.bootargs_size;
			static int copy_once = 1;

			start = end + 1; /*it is fw name for boot args */
			snprintf(dyn_bootargs, 100, " %s=0x%x@0x%x ",
				start, mod_size, mod_addr);
			dev_dbg(ACRN_DBG_BOOT, "fw-%d: %s", i, dyn_bootargs);

			/*copy boot args to load addr, set src=load addr*/
			if (copy_once != 0) {
				copy_once = 0;
				(void)strcpy_s(load_addr, MEM_2K, (const
				char *)vm->sw.linux_info.bootargs_src_addr);
				vm->sw.linux_info.bootargs_src_addr = load_addr;
			}

			(void)strcpy_s(load_addr + args_size,
				100U, dyn_bootargs);
			vm->sw.linux_info.bootargs_size =
				strnlen_s(load_addr, MEM_2K);

		} else if (strncmp("RAMDISK", start, type_len) == 0) {
			vm->sw.linux_info.ramdisk_src_addr = mod_addr;
			vm->sw.linux_info.ramdisk_load_addr =
				(void *)(uint64_t)mods[i].mm_mod_start;
			vm->sw.linux_info.ramdisk_size = mod_size;
		} else {
			pr_warn("not support mod, cmd: %s", start);
		}
	}
}

static void *get_kernel_load_addr(void *kernel_src_addr)
{
	struct zero_page *zeropage;

	/* According to the explaination for pref_address
	 * in Documentation/x86/boot.txt, a relocating
	 * bootloader should attempt to load kernel at pref_address
	 * if possible. A non-relocatable kernel will unconditionally
	 * move itself and to run at this address, so no need to copy
	 * kernel to perf_address by bootloader, if kernel is
	 * non-relocatable.
	 */
	zeropage = (struct zero_page *)kernel_src_addr;
	if (zeropage->hdr.relocatable_kernel != 0U) {
		return (void *)zeropage->hdr.pref_addr;
	}

	return kernel_src_addr;
}

/*
 * parse_image_boot_params
 *
 * description:
 *    This function parse image_boot_params from cmdline. Mainly on
 *    1. parse structure address from cmdline
 *    2. get seed_list address and call function to parse seed
 *    3. convert address in the structure from HPA to SOS's GPA
 *    4. clear original image_boot_params argument in cmdline since
 *       original address is HPA, need to convert and append later
 *       when compose kernel command line.
 *
 * input:
 *    vm            pointer to vm structure
 *    cmdline       pointer to cmdline string
 *
 * return value:
 *    boot_params   HVA of image_boot_params if parse success, or
 *                  else a NULL pointer.
 */
static void *parse_image_boot_params(struct vm *vm, char *cmdline)
{
	char *arg, *arg_end;
	char *param;
	uint32_t len;
	struct image_boot_params *boot_params;

	if (cmdline == NULL) {
		goto fail;
	}

	len = strnlen_s(boot_params_arg, MAX_BOOT_PARAMS_LEN);
	arg = strstr_s(cmdline, MEM_2K, boot_params_arg, len);
	if (arg == NULL) {
		goto fail;
	}

	param = arg + len;
	boot_params = (struct image_boot_params *)HPA2HVA(strtoul_hex(param));
	if (boot_params == NULL) {
		goto fail;
	}

	parse_seed_list((struct seed_list_hob *)HPA2HVA(
				boot_params->p_seed_list));

	/*
	 * Convert the addresses to SOS GPA since this structure will be used
	 * in SOS.
	 */
	boot_params->p_seed_list = hpa2gpa(vm, boot_params->p_seed_list);
	boot_params->p_platform_info =
				hpa2gpa(vm, boot_params->p_platform_info);

	/*
	 * Replace original arguments with spaces since SOS's GPA is not
	 * identity mapped to HPA. The argument will be appended later when
	 * compose cmdline for SOS.
	 */
	arg_end = strchr(arg, ' ');
	len = (arg_end != NULL) ? (uint32_t)(arg_end - arg) :
							strnlen_s(arg, MEM_2K);
	(void)memset(arg, ' ', len);

	return (void *)boot_params;

fail:
	parse_seed_list(NULL);
	return NULL;
}

#ifdef CONFIG_PARTITION_MODE
int init_vm_boot_info(struct vm *vm)
{
	struct multiboot_module *mods = NULL;
	struct multiboot_info *mbi = NULL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		ASSERT(false, "no multiboot info found");
		return -EINVAL;
	}

	mbi = HPA2HVA((uint64_t)boot_regs[1]);

	dev_dbg(ACRN_DBG_BOOT, "Multiboot detected, flag=0x%x", mbi->mi_flags);
	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS) == 0U) {
		ASSERT(false, "no kernel info found");
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

	/* mod[0] is for kernel&cmdline, other mod for ramdisk/firmware info*/
	mods = (struct multiboot_module *)(uint64_t)mbi->mi_mods_addr;

	dev_dbg(ACRN_DBG_BOOT, "mod0 start=0x%x, end=0x%x",
		mods[0].mm_mod_start, mods[0].mm_mod_end);
	dev_dbg(ACRN_DBG_BOOT, "cmd addr=0x%x, str=%s", mods[0].mm_string,
		(char *) (uint64_t)mods[0].mm_string);

	vm->sw.kernel_type = VM_LINUX_GUEST;
	vm->sw.kernel_info.kernel_src_addr =
			HPA2HVA((uint64_t)mods[0].mm_mod_start);
	vm->sw.kernel_info.kernel_size =
			mods[0].mm_mod_end - mods[0].mm_mod_start;

	vm->sw.kernel_info.kernel_load_addr = (void *)(16 * 1024 * 1024UL);

	vm->sw.linux_info.bootargs_src_addr =
				vm->vm_desc->bootargs;
	vm->sw.linux_info.bootargs_size =
			strnlen_s(vm->vm_desc->bootargs, MEM_2K);

	vm->sw.linux_info.bootargs_load_addr = (void *)(vm->vm_desc->mem_size -  8*1024UL);

	return 0;
}
#else
/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @return 0		- on success
 * @return -EINVAL	- on invalid parameters
 *
 * @pre vm != NULL
 * @pre is_vm0(vm) == true
 */
int init_vm_boot_info(struct vm *vm)
{
	struct multiboot_module *mods = NULL;
	struct multiboot_info *mbi = NULL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		ASSERT(false, "no multiboot info found");
		return -EINVAL;
	}

	mbi = HPA2HVA((uint64_t)boot_regs[1]);

	dev_dbg(ACRN_DBG_BOOT, "Multiboot detected, flag=0x%x", mbi->mi_flags);
	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS) == 0U) {
		ASSERT(false, "no sos kernel info found");
		return -EINVAL;
	}

	dev_dbg(ACRN_DBG_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

	/* mod[0] is for kernel&cmdline, other mod for ramdisk/firmware info*/
	mods = (struct multiboot_module *)HPA2HVA((uint64_t)mbi->mi_mods_addr);

	dev_dbg(ACRN_DBG_BOOT, "mod0 start=0x%x, end=0x%x",
		mods[0].mm_mod_start, mods[0].mm_mod_end);
	dev_dbg(ACRN_DBG_BOOT, "cmd addr=0x%x, str=%s", mods[0].mm_string,
		(char *) (uint64_t)mods[0].mm_string);

	vm->sw.kernel_type = VM_LINUX_GUEST;
	vm->sw.kernel_info.kernel_src_addr =
		HPA2HVA((uint64_t)mods[0].mm_mod_start);
	vm->sw.kernel_info.kernel_size =
		mods[0].mm_mod_end - mods[0].mm_mod_start;
	vm->sw.kernel_info.kernel_load_addr = (void *)HVA2GPA(vm, 
		get_kernel_load_addr(vm->sw.kernel_info.kernel_src_addr));

	/*
	 * If there is cmdline from mbi->mi_cmdline, merge it with
	 * mods[0].mm_string
	 */
	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) {
		char *cmd_src, *cmd_dst;
		uint32_t off = 0U;
		void *boot_params_addr;
		char buf[MAX_BOOT_PARAMS_LEN];

		cmd_dst = kernel_cmdline;
		cmd_src = HPA2HVA((uint64_t)mbi->mi_cmdline);

		boot_params_addr = parse_image_boot_params(vm, cmd_src);
		/*
		 * Convert ImageBootParamsAddr to SOS GPA and append to
		 * kernel cmdline
		 */
		if (boot_params_addr != NULL) {
			(void)memset(buf, 0U, sizeof(buf));
			snprintf(buf, MAX_BOOT_PARAMS_LEN, "%s0x%X ",
				boot_params_arg,
				HVA2GPA(vm, (uint64_t)boot_params_addr));
			(void)strncpy_s(cmd_dst, MEM_2K, buf,
						MAX_BOOT_PARAMS_LEN);
			off = strnlen_s(cmd_dst, MEM_2K);
		}

		cmd_dst += off;
		(void)strncpy_s(cmd_dst, MEM_2K - off, cmd_src,
			strnlen_s(cmd_src, MEM_2K - off));
		off = strnlen_s(cmd_dst, MEM_2K - off);
		cmd_dst[off] = ' ';	/* insert space */
		off += 1U;

		cmd_dst += off;
		cmd_src = HPA2HVA((uint64_t)mods[0].mm_string);
		(void)strncpy_s(cmd_dst, MEM_2K - off, cmd_src,
				strnlen_s(cmd_src, MEM_2K - off));

		vm->sw.linux_info.bootargs_src_addr = kernel_cmdline;
		vm->sw.linux_info.bootargs_size =
			strnlen_s(kernel_cmdline, MEM_2K);
	} else {
		vm->sw.linux_info.bootargs_src_addr =
			HPA2HVA((uint64_t)mods[0].mm_string);
		vm->sw.linux_info.bootargs_size =
			strnlen_s(HPA2HVA((uint64_t)mods[0].mm_string),
			MEM_2K);
	}

	vm->sw.linux_info.bootargs_load_addr = (void *)BOOT_ARGS_LOAD_ADDR;

	if (mbi->mi_mods_count > 1U) {
		/*parse other modules, like firmware /ramdisk */
		parse_other_modules(vm, mods + 1, mbi->mi_mods_count - 1);
	}
	return 0;
}
#endif
