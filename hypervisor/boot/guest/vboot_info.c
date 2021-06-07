/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <asm/per_cpu.h>
#include <asm/irq.h>
#include <boot.h>
#include <asm/pgtable.h>
#include <asm/zeropage.h>
#include <asm/seed.h>
#include <asm/mmu.h>
#include <asm/guest/vm.h>
#include <asm/guest/ept.h>
#include <reloc.h>
#include <logmsg.h>
#include <vboot_info.h>
#include <vacpi.h>

#define DBG_LEVEL_BOOT	6U

/* TODO:
 * The value is referenced from Linux boot protocal for old kernels,
 * but this should be configurable for different OS. */
#define DEFAULT_RAMDISK_GPA_MAX		0x37ffffffUL

#define PRE_VM_MAX_RAM_ADDR_BELOW_4GB		(VIRT_ACPI_DATA_ADDR - 1U)

/**
 * @pre vm != NULL && mod != NULL
 */
static void init_vm_ramdisk_info(struct acrn_vm *vm, const struct abi_module *mod)
{
	uint64_t ramdisk_load_gpa = INVALID_GPA;
	uint64_t ramdisk_gpa_max = DEFAULT_RAMDISK_GPA_MAX;
	uint64_t kernel_start = (uint64_t)vm->sw.kernel_info.kernel_load_addr;
	uint64_t kernel_end = kernel_start + vm->sw.kernel_info.kernel_size;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	if (mod->start != NULL) {
		vm->sw.ramdisk_info.src_addr = mod->start;
		vm->sw.ramdisk_info.size = mod->size;
	}

	/* Per Linux boot protocol, the Kernel need a size of contiguous
	 * memory(i.e. init_size field in zeropage) from its extract address to boot,
	 * and initrd_addr_max field specifies the maximum address of the ramdisk.
	 * Per kernel src head_64.S, decompressed kernel start at 2M aligned to the
	 * compressed kernel load address.
	 */
	if (vm->sw.kernel_type == KERNEL_BZIMAGE) {
		struct zero_page *zeropage = (struct zero_page *)vm->sw.kernel_info.kernel_src_addr;
		uint32_t kernel_init_size = zeropage->hdr.init_size;
		uint32_t initrd_addr_max = zeropage->hdr.initrd_addr_max;

		kernel_end = kernel_start + MEM_2M + kernel_init_size;
		if (initrd_addr_max != 0U) {
			ramdisk_gpa_max = initrd_addr_max;
		}
	}

	if (is_sos_vm(vm)) {
		uint64_t mods_start, mods_end;

		get_boot_mods_range(&mods_start, &mods_end);
		mods_start = sos_vm_hpa2gpa(mods_start);
		mods_end = sos_vm_hpa2gpa(mods_end);

		if (vm->sw.ramdisk_info.src_addr != NULL) {
			ramdisk_load_gpa = sos_vm_hpa2gpa((uint64_t)vm->sw.ramdisk_info.src_addr);
		}

		/* For SOS VM, the ramdisk has been loaded by bootloader, so in most cases
		 * there is no need to do gpa copy again. But in the case that the ramdisk is
		 * loaded by bootloader at a address higher than its limit, we should do gpa
		 * copy then.
		 */
		if ((ramdisk_load_gpa + vm->sw.ramdisk_info.size) > ramdisk_gpa_max) {
			/* In this case, mods_end must be higher than ramdisk_gpa_max,
			 * so try to locate ramdisk between MEM_1M and mods_start/kernel_start,
			 * or try the range between kernel_end and mods_start;
			 */
			ramdisk_load_gpa = find_space_from_ve820(vm, vm->sw.ramdisk_info.size,
					MEM_1M, min(min(mods_start, kernel_start), ramdisk_gpa_max));
			if ((ramdisk_load_gpa == INVALID_GPA) && (kernel_end < min(mods_start, ramdisk_gpa_max))) {
				ramdisk_load_gpa = find_space_from_ve820(vm, vm->sw.ramdisk_info.size,
						kernel_end, min(mods_start, ramdisk_gpa_max));
			}
		}
	} else {
		/* For pre-launched VM, the ramdisk would be put by searching ve820 table.
		 */
		ramdisk_gpa_max = min(PRE_VM_MAX_RAM_ADDR_BELOW_4GB, ramdisk_gpa_max);

		if (kernel_end < ramdisk_gpa_max) {
			ramdisk_load_gpa = find_space_from_ve820(vm, vm->sw.ramdisk_info.size,
					kernel_end, ramdisk_gpa_max);
		}
		if (ramdisk_load_gpa == INVALID_GPA) {
			ramdisk_load_gpa = find_space_from_ve820(vm, vm->sw.ramdisk_info.size,
					MEM_1M, min(kernel_start, ramdisk_gpa_max));
		}
	}

	if (ramdisk_load_gpa == INVALID_GPA) {
		pr_err("no space in guest memory to load VM %d ramdisk", vm->vm_id);
		vm->sw.ramdisk_info.size = 0U;
	}

	/* Use customer specified ramdisk load addr if it is configured in VM configuration,
	 * otherwise use allocated address calculated by HV.
	 */
	if (vm_config->os_config.kernel_ramdisk_addr != 0UL) {
		vm->sw.ramdisk_info.load_addr = (void *)vm_config->os_config.kernel_ramdisk_addr;
	} else {
		vm->sw.ramdisk_info.load_addr = (void *)ramdisk_load_gpa;
	}

	dev_dbg(DBG_LEVEL_BOOT, "ramdisk mod start=0x%x, size=0x%x", (uint64_t)mod->start, mod->size);
	dev_dbg(DBG_LEVEL_BOOT, "ramdisk load addr = 0x%lx", ramdisk_load_gpa);
}

/**
 * @pre vm != NULL && mod != NULL
 */
static void init_vm_acpi_info(struct acrn_vm *vm, const struct abi_module *mod)
{
	vm->sw.acpi_info.src_addr = mod->start;
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
		 * move itself and to run at this address.
		 */
		zeropage = (struct zero_page *)sw_info->kernel_info.kernel_src_addr;

		if ((is_sos_vm(vm)) && (zeropage->hdr.relocatable_kernel != 0U)) {
			uint64_t mods_start, mods_end;
			uint64_t kernel_load_gpa = INVALID_GPA;
			uint32_t kernel_align = zeropage->hdr.kernel_alignment;
			uint32_t kernel_init_size = zeropage->hdr.init_size;
			/* Because the kernel load address need to be up aligned to kernel_align size
			 * whereas find_space_from_ve820() can only return page aligned address,
			 * we enlarge the needed size to (kernel_init_size + 2 * kernel_align).
			 */
			uint32_t kernel_size = kernel_init_size + 2 * kernel_align;

			get_boot_mods_range(&mods_start, &mods_end);
			mods_start = sos_vm_hpa2gpa(mods_start);
			mods_end = sos_vm_hpa2gpa(mods_end);

			/* TODO: support load kernel when modules are beyond 4GB space. */
			if (mods_end < MEM_4G) {
				kernel_load_gpa = find_space_from_ve820(vm, kernel_size, MEM_1M, mods_start);

				if (kernel_load_gpa == INVALID_GPA) {
					kernel_load_gpa = find_space_from_ve820(vm, kernel_size, mods_end, MEM_4G);
				}
			}

			if (kernel_load_gpa != INVALID_GPA) {
				load_addr = (void *)roundup((uint64_t)kernel_load_gpa, kernel_align);
			}
		} else {
			load_addr = (void *)zeropage->hdr.pref_addr;
			if (is_sos_vm(vm)) {
				/* The non-relocatable SOS kernel might overlap with boot modules. */
				pr_err("Non-relocatable kernel found, risk to boot!");
			}
		}
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

	dev_dbg(DBG_LEVEL_BOOT, "VM%d kernel load_addr: 0x%lx", vm->vm_id, load_addr);
	return load_addr;
}

/**
 * @pre vm != NULL && mod != NULL
 */
static int32_t init_vm_kernel_info(struct acrn_vm *vm, const struct abi_module *mod)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	dev_dbg(DBG_LEVEL_BOOT, "kernel mod start=0x%x, size=0x%x",
			(uint64_t)mod->start, mod->size);

	vm->sw.kernel_type = vm_config->os_config.kernel_type;
	vm->sw.kernel_info.kernel_src_addr = mod->start;
	if (vm->sw.kernel_info.kernel_src_addr != NULL) {
		vm->sw.kernel_info.kernel_size = mod->size;
		vm->sw.kernel_info.kernel_load_addr = get_kernel_load_addr(vm);
	}

	return (vm->sw.kernel_info.kernel_load_addr == NULL) ? (-EINVAL) : 0;
}

/* cmdline parsed from abi module string, for pre-launched VMs and SOS VM only. */
static char mod_cmdline[PRE_VM_NUM + SOS_VM_NUM][MAX_BOOTARGS_SIZE] = { '\0' };

/**
 * @pre vm != NULL && abi != NULL
 */
static void init_vm_bootargs_info(struct acrn_vm *vm, const struct acrn_boot_info *abi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	vm->sw.bootargs_info.src_addr = vm_config->os_config.bootargs;
	/* If module string of the kernel module exists, it would OVERRIDE the pre-configured build-in VM bootargs,
	 * which means we give user a chance to re-configure VM bootargs at bootloader runtime. e.g. GRUB menu
	 */
	if (mod_cmdline[vm->vm_id][0] != '\0') {
		vm->sw.bootargs_info.src_addr = &mod_cmdline[vm->vm_id][0];
	}

	if (vm_config->load_order == SOS_VM) {
		if (strncat_s((char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE, " ", 1U) == 0) {
			char seed_args[MAX_SEED_ARG_SIZE] = "";

			fill_seed_arg(seed_args, MAX_SEED_ARG_SIZE);
			/* Fill seed argument for SOS
			 * seed_args string ends with a white space and '\0', so no additional delimiter is needed
			 */
			if (strncat_s((char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE,
					seed_args, (MAX_BOOTARGS_SIZE - 1U)) != 0) {
				pr_err("failed to fill seed arg to SOS bootargs!");
			}

			/* If there is cmdline from abi->cmdline, merge it with configured SOS bootargs.
			 * This is very helpful when one of configured bootargs need to be revised at GRUB runtime
			 * (e.g. "root="), since the later one would override the previous one if multiple bootargs exist.
			 */
			if (abi->cmdline[0] != '\0') {
				if (strncat_s((char *)vm->sw.bootargs_info.src_addr, MAX_BOOTARGS_SIZE,
						abi->cmdline, (MAX_BOOTARGS_SIZE - 1U)) != 0) {
					pr_err("failed to merge mbi cmdline to SOS bootargs!");
				}
			}
		} else {
			pr_err("no space to append SOS bootargs!");
		}

	}

	vm->sw.bootargs_info.size = strnlen_s((const char *)vm->sw.bootargs_info.src_addr, (MAX_BOOTARGS_SIZE - 1U)) + 1U;

}

/* @pre abi != NULL && tag != NULL
 */
static struct abi_module *get_mod_by_tag(const struct acrn_boot_info *abi, const char *tag)
{
	uint8_t i;
	struct abi_module *mod = NULL;
	struct abi_module *mods = (struct abi_module *)(&abi->mods[0]);
	uint32_t tag_len = strnlen_s(tag, MAX_MOD_TAG_LEN);

	for (i = 0U; i < abi->mods_count; i++) {
		const char *string = (char *)hpa2hva((uint64_t)(mods + i)->string);
		uint32_t str_len = strnlen_s(string, MAX_MOD_TAG_LEN);
		const char *p_chr = string + tag_len; /* point to right after the end of tag */

		/* The tag must be located at the first word in string and end with SPACE/TAB or EOL since
		 * when do file stitch by tool, the tag in string might be followed by EOL(0x0d/0x0a).
		 */
		if ((str_len >= tag_len) && (strncmp(string, tag, tag_len) == 0)
				&& (is_space(*p_chr) || is_eol(*p_chr))) {
			mod = mods + i;
			break;
		}
	}
	/* GRUB might put module at address 0 or under 1MB in the case that the module size is less then 1MB
	 * ACRN will not support these cases
	 */
	if ((mod != NULL) && (mod->start == NULL)) {
		pr_err("Unsupported module: start at HPA 0, size 0x%x .", mod->size);
		mod = NULL;
	}

	return mod;
}

/* @pre vm != NULL && abi != NULL
 */
static int32_t init_vm_sw_load(struct acrn_vm *vm, const struct acrn_boot_info *abi)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct abi_module *mod;
	int32_t ret = -EINVAL;

	dev_dbg(DBG_LEVEL_BOOT, "mod counts=%d\n", abi->mods_count);

	/* find kernel module first */
	mod = get_mod_by_tag(abi, vm_config->os_config.kernel_mod_tag);
	if (mod != NULL) {
		const char *string = (char *)hpa2hva((uint64_t)mod->string);
		uint32_t str_len = strnlen_s(string, MAX_BOOTARGS_SIZE);
		uint32_t tag_len = strnlen_s(vm_config->os_config.kernel_mod_tag, MAX_MOD_TAG_LEN);
		const char *p_chr = string + tag_len + 1; /* point to the possible start of cmdline */

		/* check whether there is a cmdline configured in module string */
		if (((str_len > (tag_len + 1U))) && (is_space(*(p_chr - 1))) && (!is_eol(*p_chr))) {
			(void)strncpy_s(&mod_cmdline[vm->vm_id][0], MAX_BOOTARGS_SIZE,
					p_chr, (MAX_BOOTARGS_SIZE - 1U));
		}

		ret = init_vm_kernel_info(vm, mod);
	}

	if (ret == 0) {
		/* Currently VM bootargs only support Linux guest */
		if (vm->sw.kernel_type == KERNEL_BZIMAGE) {
			init_vm_bootargs_info(vm, abi);
		}
		/* check whether there is a ramdisk module */
		mod = get_mod_by_tag(abi, vm_config->os_config.ramdisk_mod_tag);
		if (mod != NULL) {
			init_vm_ramdisk_info(vm, mod);
		}

		if (is_prelaunched_vm(vm)) {
			mod = get_mod_by_tag(abi, vm_config->acpi_config.acpi_mod_tag);
			if ((mod != NULL) && (mod->size == ACPI_MODULE_SIZE)) {
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
	struct acrn_boot_info *abi = get_acrn_boot_info();
	int32_t ret = -EINVAL;

	stac();
	ret = init_vm_sw_load(vm, abi);
	clac();

	return ret;
}
