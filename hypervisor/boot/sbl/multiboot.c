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
#include <bsp_cfg.h>
#include <bsp_extern.h>
#include <acrn_hv_defs.h>
#include <hv_debug.h>
#include <multiboot.h>
#include <zeropage.h>

#define BOOT_ARGS_LOAD_ADDR				0x24EFC000

#define ACRN_DBG_BOOT	6

/*now modules support: FIRMWARE & RAMDISK */
static void parse_other_modules(struct vm *vm,
	struct multiboot_module *mods, int mods_count)
{
	int i = 0;

	for (i = 0; i < mods_count; i++) {
		int type_len = 0;
		const char *start = (const char *) (uint64_t)mods[i].mm_string;
		const char *end;
		void *mod_addr = (void *)(uint64_t)mods[i].mm_mod_start;
		uint32_t mod_size = mods[i].mm_mod_end - mods[i].mm_mod_start;

		dev_dbg(ACRN_DBG_BOOT, "other mod-%d start=0x%x, end=0x%x",
			i, mods[i].mm_mod_start, mods[i].mm_mod_end);
		dev_dbg(ACRN_DBG_BOOT, "cmd addr=0x%x, str=%s",
			mods[i].mm_string, start);

		while (*start == ' ')
			start++;

		end = start;
		while (*end != ' ' && *end)
			end++;

		type_len = end - start;
		if (strncmp("FIRMWARE", start, type_len) == 0) {
			char  dyn_bootargs[100] = {0};
			void *load_addr = vm->sw.linux_info.bootargs_load_addr;
			uint32_t args_size = vm->sw.linux_info.bootargs_size;
			static int copy_once = 1;

			start = end + 1; /*it is fw name for boot args */
			snprintf(dyn_bootargs, 100, " %s=0x%x@0x%x ",
				start, mod_size, mod_addr);
			dev_dbg(ACRN_DBG_BOOT, "fw-%d: %s", i, dyn_bootargs);

			/*copy boot args to load addr, set src=load addr*/
			if (copy_once) {
				copy_once = 0;
				strcpy_s(load_addr, MEM_2K,
					vm->sw.linux_info.bootargs_src_addr);
				vm->sw.linux_info.bootargs_src_addr = load_addr;
			}

			strcpy_s(load_addr + args_size - 1,
				100, dyn_bootargs);
			vm->sw.linux_info.bootargs_size =
				strnlen_s(load_addr, MEM_2K);

		} else if (strncmp("RAMDISK", start, type_len) == 0) {
			vm->sw.linux_info.ramdisk_src_addr = mod_addr;
			vm->sw.linux_info.ramdisk_load_addr = mod_addr;
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
	if (zeropage->hdr.relocatable_kernel)
		return (void *)zeropage->hdr.pref_addr;

	return kernel_src_addr;
}

int init_vm0_boot_info(struct vm *vm)
{
	struct multiboot_module *mods = NULL;
	struct multiboot_info *mbi = NULL;

	if (!is_vm0(vm)) {
		pr_err("just for vm0 to get info!");
		return -EINVAL;
	}

	if (boot_regs[0] != MULTIBOOT_INFO_MAGIC) {
		ASSERT(0, "no multiboot info found");
		return -EINVAL;
	}

	mbi = (struct multiboot_info *)((uint64_t)boot_regs[1]);

	dev_dbg(ACRN_DBG_BOOT, "Multiboot detected, flag=0x%x", mbi->mi_flags);
	if (!(mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS)) {
		ASSERT(0, "no sos kernel info found");
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
		(void *)(uint64_t)mods[0].mm_mod_start;
	vm->sw.kernel_info.kernel_size =
		mods[0].mm_mod_end - mods[0].mm_mod_start;
	vm->sw.kernel_info.kernel_load_addr =
		get_kernel_load_addr(vm->sw.kernel_info.kernel_src_addr);

	vm->sw.linux_info.bootargs_src_addr =
		(void *)(uint64_t)mods[0].mm_string;
	vm->sw.linux_info.bootargs_load_addr =
		(void *)BOOT_ARGS_LOAD_ADDR;
	vm->sw.linux_info.bootargs_size =
		strnlen_s((char *)(uint64_t) mods[0].mm_string, MEM_2K);

	if (mbi->mi_mods_count > 1) {
		/*parse other modules, like firmware /ramdisk */
		parse_other_modules(vm, mods + 1, mbi->mi_mods_count - 1);
	}
	return 0;
}
