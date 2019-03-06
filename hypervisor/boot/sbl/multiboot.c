/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <multiboot.h>
#include <zeropage.h>
#include <seed.h>

#define ACRN_DBG_BOOT	6U

#define MAX_BOOT_PARAMS_LEN 64U

/* There are two sources for sos_vm kernel cmdline:
 * - cmdline from sbl. mbi->cmdline
 * - cmdline from acrn stitching tool. mod[0].mm_string
 * We need to merge them together
 */
static char kernel_cmdline[MEM_2K];

/* now modules support: FIRMWARE & RAMDISK & SeedList */
static void parse_other_modules(struct acrn_vm *vm, const struct multiboot_module *mods, uint32_t mods_count)
{
	uint32_t i;

	for (i = 0U; i < mods_count; i++) {
		uint32_t type_len;
		const char *start = (char *)hpa2hva((uint64_t)mods[i].mm_string);
		const char *end;
		void *mod_addr = hpa2hva((uint64_t)mods[i].mm_mod_start);
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
			void *load_addr = gpa2hva(vm, (uint64_t)vm->sw.linux_info.bootargs_load_addr);
			uint32_t args_size = vm->sw.linux_info.bootargs_size;
			static int32_t copy_once = 1;

			start = end + 1; /*it is fw name for boot args */
			snprintf(dyn_bootargs, 100U, " %s=0x%x@0x%x ", start, mod_size, mod_addr);
			dev_dbg(ACRN_DBG_BOOT, "fw-%d: %s", i, dyn_bootargs);

			/*copy boot args to load addr, set src=load addr*/
			if (copy_once != 0) {
				copy_once = 0;
				(void)strncpy_s(load_addr, MEM_2K, (const char *)vm->sw.linux_info.bootargs_src_addr,
					vm->sw.linux_info.bootargs_size);
				vm->sw.linux_info.bootargs_src_addr = load_addr;
			}

			(void)strncpy_s(load_addr + args_size, 100U, dyn_bootargs, 100U);
			vm->sw.linux_info.bootargs_size = strnlen_s(load_addr, MEM_2K);

		} else if (strncmp("RAMDISK", start, type_len) == 0) {
			vm->sw.linux_info.ramdisk_src_addr = mod_addr;
			vm->sw.linux_info.ramdisk_load_addr = vm->sw.kernel_info.kernel_load_addr +
				vm->sw.kernel_info.kernel_size;
			vm->sw.linux_info.ramdisk_load_addr =
				(void *)round_page_up((uint64_t)vm->sw.linux_info.ramdisk_load_addr);
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
		zeropage = (struct zero_page *)zeropage->hdr.pref_addr;
	}

	return (void *)zeropage;
}

/**
 * @param[inout] vm pointer to a vm descriptor
 *
 * @retval 0 on success
 * @retval -EINVAL on invalid parameters
 *
 * @pre vm != NULL
 * @pre is_sos_vm(vm) == true
 */
int32_t sbl_init_vm_boot_info(struct acrn_vm *vm)
{
	struct multiboot_module *mods = NULL;
	struct multiboot_info *mbi = NULL;
	int32_t ret = -EINVAL;

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		panic("no multiboot info found");
	} else {
		mbi = (struct multiboot_info *)hpa2hva((uint64_t)boot_regs[1]);

		if (mbi != NULL) {
			stac();
			dev_dbg(ACRN_DBG_BOOT, "Multiboot detected, flag=0x%x", mbi->mi_flags);
			if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS) == 0U) {
				panic("no sos kernel info found");
				clac();
			} else {

				dev_dbg(ACRN_DBG_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

				/* mod[0] is for kernel&cmdline, other mod for ramdisk/firmware info*/
				mods = (struct multiboot_module *)hpa2hva((uint64_t)mbi->mi_mods_addr);

				dev_dbg(ACRN_DBG_BOOT, "mod0 start=0x%x, end=0x%x",
					mods[0].mm_mod_start, mods[0].mm_mod_end);
				dev_dbg(ACRN_DBG_BOOT, "cmd addr=0x%x, str=%s", mods[0].mm_string,
					(char *)(uint64_t)mods[0].mm_string);

				vm->sw.kernel_type = VM_LINUX_GUEST;
				vm->sw.kernel_info.kernel_src_addr = hpa2hva((uint64_t)mods[0].mm_mod_start);
				vm->sw.kernel_info.kernel_size = mods[0].mm_mod_end - mods[0].mm_mod_start;

				struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

				if (vm_config->type == PRE_LAUNCHED_VM) {
					vm->sw.kernel_info.kernel_load_addr = (void *)(MEM_1M * 16U);
					vm->sw.linux_info.bootargs_src_addr = (void *)vm_config->os_config.bootargs;
					vm->sw.linux_info.bootargs_size =
						strnlen_s(vm_config->os_config.bootargs, MEM_2K);
				} else {
					vm->sw.kernel_info.kernel_load_addr =
						get_kernel_load_addr(vm->sw.kernel_info.kernel_src_addr);

					/*
					 * If there is cmdline from mbi->mi_cmdline, merge it with
					 * mods[0].mm_string
					 */
					if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) {
						char *cmd_src, *cmd_dst;
						uint32_t off = 0U;

						cmd_dst = kernel_cmdline;
						cmd_src = (char *)hpa2hva((uint64_t)mbi->mi_cmdline);

						/*
						 * Append seed argument for SOS
						 */
						append_seed_arg(cmd_dst, is_sos_vm(vm));

						off = strnlen_s(cmd_dst, MEM_2K);

						cmd_dst += off;
						(void)strncpy_s(cmd_dst, MEM_2K - off, (const char *)cmd_src,
							strnlen_s(cmd_src, MEM_2K - off));
						off = strnlen_s(cmd_dst, MEM_2K - off);
						cmd_dst[off] = ' ';	/* insert space */
						off += 1U;

						cmd_dst += off;
						cmd_src = (char *)hpa2hva((uint64_t)mods[0].mm_string);
						(void)strncpy_s(cmd_dst, MEM_2K - off, cmd_src,
							strnlen_s(cmd_src, MEM_2K - off));

						vm->sw.linux_info.bootargs_src_addr = kernel_cmdline;
						vm->sw.linux_info.bootargs_size = strnlen_s(kernel_cmdline, MEM_2K);
					} else {
						vm->sw.linux_info.bootargs_src_addr =
							hpa2hva((uint64_t)mods[0].mm_string);
						vm->sw.linux_info.bootargs_size =
							strnlen_s(hpa2hva((uint64_t)mods[0].mm_string), MEM_2K);
					}
				}

				/* Kernel bootarg and zero page are right before the kernel image */
				vm->sw.linux_info.bootargs_load_addr =
					vm->sw.kernel_info.kernel_load_addr - (MEM_1K * 8U);

				if (mbi->mi_mods_count > 1U) {
					/*parse other modules, like firmware /ramdisk */
					parse_other_modules(vm, mods + 1, mbi->mi_mods_count - 1);
				}
				clac();
				ret = 0;
			}
		}
	}
	return ret;
}
