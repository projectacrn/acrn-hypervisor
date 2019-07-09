/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <per_cpu.h>
#include <irq.h>
#include <boot_context.h>
#include <sprintf.h>
#include <multiboot.h>
#include <pgtable.h>
#include <zeropage.h>
#include <seed.h>
#include <mmu.h>
#include <vm.h>
#include <logmsg.h>
#include <deprivilege_boot.h>

#define ACRN_DBG_BOOT	6U

#define MAX_BOOT_PARAMS_LEN 64U
#define INVALID_MOD_IDX		0xFFFFU

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

/* There are two sources for sos_vm kernel cmdline:
 * - cmdline from direct boot mbi->cmdline
 * - cmdline from acrn stitching tool. mod[0].mm_string
 * We need to merge them together
 */
static char kernel_cmdline[MAX_BOOTARGS_SIZE + 1U];

/**
 * @pre vm != NULL && cmdline != NULL && cmdstr != NULL
 */
static void merge_cmdline(const struct acrn_vm *vm, const char *cmdline, const char *cmdstr)
{
	char *cmd_dst = kernel_cmdline;
	uint32_t cmdline_len, cmdstr_len;
	uint32_t dst_avail; /* available room for cmd_dst[] */
	uint32_t dst_len; /* the actual number of characters that are copied */

	/*
	 * Append seed argument for SOS
	 * seed_arg string ends with a white space and '\0', so no aditional delimiter is needed
	 */
	append_seed_arg(cmd_dst, is_sos_vm(vm));
	dst_len = strnlen_s(cmd_dst, MAX_BOOTARGS_SIZE);
	dst_avail = MAX_BOOTARGS_SIZE + 1U - dst_len;
	cmd_dst += dst_len;

	cmdline_len = strnlen_s(cmdline, MAX_BOOTARGS_SIZE);
	cmdstr_len = strnlen_s(cmdstr, MAX_BOOTARGS_SIZE);

	/* reserve one character for the delimiter between 2 strings (one white space) */
	if ((cmdline_len + cmdstr_len + 1U) >= dst_avail) {
		panic("Multiboot bootarg string too long");
	} else {
		/* copy mbi->mi_cmdline */
		(void)strncpy_s(cmd_dst, dst_avail, cmdline, cmdline_len);
		dst_len = strnlen_s(cmd_dst, dst_avail);
		dst_avail -= dst_len;
		cmd_dst += dst_len;

		/* overwrite '\0' with a white space */
		(void)strncpy_s(cmd_dst, dst_avail, " ", 1U);
		dst_avail -= 1U;
		cmd_dst += 1U;

		/* copy vm_config->os_config.bootargs */
		(void)strncpy_s(cmd_dst, dst_avail, cmdstr, cmdstr_len);
	}
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

	dev_dbg(ACRN_DBG_BOOT, "kernel mod start=0x%x, end=0x%x",
		mod->mm_mod_start, mod->mm_mod_end);

	vm->sw.kernel_type = vm_config->os_config.kernel_type;
	vm->sw.kernel_info.kernel_src_addr = hpa2hva((uint64_t)mod->mm_mod_start);
	if ((vm->sw.kernel_info.kernel_src_addr != NULL) && (mod->mm_mod_end > mod->mm_mod_start)){
		vm->sw.kernel_info.kernel_size = mod->mm_mod_end - mod->mm_mod_start;
		vm->sw.kernel_info.kernel_load_addr = get_kernel_load_addr(vm);
	}

	return (vm->sw.kernel_info.kernel_load_addr == NULL) ? (-EINVAL) : 0;
}

/**
 * @pre vm != NULL && mbi != NULL
 */
static void init_vm_bootargs_info(struct acrn_vm *vm, const struct multiboot_info *mbi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	char *bootargs = vm_config->os_config.bootargs;

	if (vm_config->load_order == PRE_LAUNCHED_VM) {
		vm->sw.bootargs_info.src_addr = bootargs;
		vm->sw.bootargs_info.size = strnlen_s(bootargs, MAX_BOOTARGS_SIZE);
	} else {
		/* vm_config->load_order == SOS_VM */
		if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) {
			/*
			 * If there is cmdline from mbi->mi_cmdline, merge it with
			 * vm_config->os_config.bootargs
			 */
			merge_cmdline(vm, hpa2hva((uint64_t)mbi->mi_cmdline), bootargs);

			vm->sw.bootargs_info.src_addr = kernel_cmdline;
			vm->sw.bootargs_info.size = strnlen_s(kernel_cmdline, MAX_BOOTARGS_SIZE);
		} else {
			vm->sw.bootargs_info.src_addr = bootargs;
			vm->sw.bootargs_info.size = strnlen_s(bootargs, MAX_BOOTARGS_SIZE);
		}
	}

	/* Kernel bootarg and zero page are right before the kernel image */
	if (vm->sw.bootargs_info.size > 0U) {
		vm->sw.bootargs_info.load_addr = vm->sw.kernel_info.kernel_load_addr - (MEM_1K * 8U);
	} else {
		vm->sw.bootargs_info.load_addr = NULL;
	}
}

/* @pre mods != NULL
 */
static uint32_t get_mod_idx_by_tag(const struct multiboot_module *mods, uint32_t mods_count, const char *tag)
{
	uint32_t i, ret = INVALID_MOD_IDX;
	uint32_t tag_len = strnlen_s(tag, MAX_MOD_TAG_LEN);

	for (i = 0U; i < mods_count; i++) {
		const char *mm_string = (char *)hpa2hva((uint64_t)mods[i].mm_string);
		uint32_t mm_str_len = strnlen_s(mm_string, MAX_MOD_TAG_LEN);

		/* when do file stitch by tool, the tag in mm_string might be followed with 0x0d or 0x0a */
		if ((mm_str_len >= tag_len) && (strncmp(mm_string, tag, tag_len) == 0)
				&& ((*(mm_string + tag_len) == 0x0d)
				|| (*(mm_string + tag_len) == 0x0a)
				|| (*(mm_string + tag_len) == 0))){
			ret = i;
			break;
		}
	}
	return ret;
}

/* @pre vm != NULL && mbi != NULL
 */
static int32_t init_vm_sw_load(struct acrn_vm *vm, const struct multiboot_info *mbi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct multiboot_module *mods = (struct multiboot_module *)hpa2hva((uint64_t)mbi->mi_mods_addr);
	uint32_t mod_idx;
	int32_t ret = -EINVAL;

	dev_dbg(ACRN_DBG_BOOT, "mod counts=%d\n", mbi->mi_mods_count);

	if (mods != NULL) {
		mod_idx = get_mod_idx_by_tag(mods, mbi->mi_mods_count, vm_config->os_config.kernel_mod_tag);
		if (mod_idx != INVALID_MOD_IDX) {
			ret = init_vm_kernel_info(vm, &mods[mod_idx]);
		}
	}

	if (ret == 0) {
		init_vm_bootargs_info(vm, mbi);
		/* check whether there is a ramdisk module */
		mod_idx = get_mod_idx_by_tag(mods, mbi->mi_mods_count, vm_config->os_config.ramdisk_mod_tag);
		if (mod_idx != INVALID_MOD_IDX) {
			init_vm_ramdisk_info(vm, &mods[mod_idx]);
			/* TODO: prepare other modules like firmware, seedlist */
		}
	} else {
		pr_err("failed to load VM %d kernel module", vm->vm_id);
	}
	return ret;
}

/**
 * @pre vm != NULL
 */
static int32_t init_general_vm_boot_info(struct acrn_vm *vm)
{
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
				panic("no multiboot module info found");
			} else {
				ret = init_vm_sw_load(vm, mbi);
			}
			clac();
		}
	}
	return ret;
}

static void depri_boot_spurious_handler(uint32_t vector)
{
	if (get_pcpu_id() == BOOT_CPU_ID) {
		struct acrn_vcpu *vcpu = per_cpu(vcpu, BOOT_CPU_ID);

		if (vcpu != NULL) {
			vlapic_set_intr(vcpu, vector, LAPIC_TRIG_EDGE);
		} else {
			pr_err("%s vcpu or vlapic is not ready, interrupt lost\n", __func__);
		}
	}
}

static int32_t depri_boot_sw_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BOOT_CPU_ID);
	struct acrn_vcpu_regs *vcpu_regs = &boot_context;
	const struct depri_boot_context *depri_boot_ctx = get_depri_boot_ctx();
	const struct lapic_regs *depri_boot_lapic_regs = get_depri_boot_lapic_regs();

	pr_dbg("Loading guest to run-time location");

	vlapic_restore(vcpu_vlapic(vcpu), depri_boot_lapic_regs);

	/* For UEFI platform, the bsp init regs come from two places:
	 * 1. saved in depri_boot: gpregs, rip
	 * 2. saved when HV started: other registers
	 * We copy the info saved in depri_boot to boot_context and
	 * init bsp with boot_context.
	 */
	(void)memcpy_s((void *)&(vcpu_regs->gprs), sizeof(struct acrn_gp_regs),
		&(depri_boot_ctx->vcpu_regs.gprs), sizeof(struct acrn_gp_regs));

	vcpu_regs->rip = depri_boot_ctx->vcpu_regs.rip;
	set_vcpu_regs(vcpu, vcpu_regs);

	/* defer irq enabling till vlapic is ready */
	spurious_handler = depri_boot_spurious_handler;
	CPU_IRQ_ENABLE();

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
	int32_t ret = 0;

	if (is_sos_vm(vm) && (get_sos_boot_mode() == DEPRI_BOOT_MODE)) {
		vm_sw_loader = depri_boot_sw_loader;
	} else {
		vm_sw_loader = direct_boot_sw_loader;
		ret = init_general_vm_boot_info(vm);
	}

	return ret;
}
