/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <x86/e820.h>
#include <x86/zeropage.h>
#include <x86/guest/ept.h>
#include <x86/mmu.h>
#include <multiboot.h>
#include <errno.h>
#include <sprintf.h>
#include <logmsg.h>

#define NUM_REMAIN_1G_PAGES	3UL

/*
 * We put the guest init gdt after kernel/bootarg/ramdisk images. Suppose this is a
 * safe place for guest init gdt of guest whatever the configuration is used by guest.
 */
static uint64_t get_guest_gdt_base_gpa(const struct acrn_vm *vm)
{
	uint64_t new_guest_gdt_base_gpa, guest_kernel_end_gpa, guest_bootargs_end_gpa, guest_ramdisk_end_gpa;

	guest_kernel_end_gpa = (uint64_t)vm->sw.kernel_info.kernel_load_addr + vm->sw.kernel_info.kernel_size;
	guest_bootargs_end_gpa = (uint64_t)vm->sw.bootargs_info.load_addr + vm->sw.bootargs_info.size;
	guest_ramdisk_end_gpa = (uint64_t)vm->sw.ramdisk_info.load_addr + vm->sw.ramdisk_info.size;

	new_guest_gdt_base_gpa = max(guest_kernel_end_gpa, max(guest_bootargs_end_gpa, guest_ramdisk_end_gpa));
	new_guest_gdt_base_gpa = (new_guest_gdt_base_gpa + 7UL) & ~(8UL - 1UL);

	return new_guest_gdt_base_gpa;
}

/**
 * @pre zp != NULL && vm != NULL
 */
static uint32_t create_zeropage_e820(struct zero_page *zp, const struct acrn_vm *vm)
{
	uint32_t entry_num = vm->e820_entry_num;
	struct e820_entry *zp_e820 = zp->entries;
	const struct e820_entry *vm_e820 = vm->e820_entries;

	if ((zp_e820 == NULL) || (vm_e820 == NULL) || (entry_num == 0U) || (entry_num > E820_MAX_ENTRIES)) {
		pr_err("e820 create error");
		entry_num = 0U;
	} else {
		(void)memcpy_s((void *)zp_e820, entry_num * sizeof(struct e820_entry),
			(void *)vm_e820, entry_num * sizeof(struct e820_entry));
	}
	return entry_num;
}

/**
 * @pre vm != NULL
 * @pre (vm->min_mem_addr <= kernel_load_addr) && (kernel_load_addr < vm->max_mem_addr)
 */
static uint64_t create_zero_page(struct acrn_vm *vm)
{
	struct zero_page *zeropage, *hva;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	struct sw_module_info *ramdisk_info = &(vm->sw.ramdisk_info);
	uint64_t gpa, addr;

	/* Set zeropage in Linux Guest RAM region just past boot args */
	gpa = (uint64_t)bootargs_info->load_addr + MEM_4K;
	hva = (struct zero_page *)gpa2hva(vm, gpa);
	zeropage = hva;

	stac();
	/* clear the zeropage */
	(void)memset(zeropage, 0U, MEM_2K);

#ifdef CONFIG_MULTIBOOT2
	if (is_sos_vm(vm)) {
		struct acrn_multiboot_info *mbi = get_acrn_multiboot_info();

		(void)memcpy_s(&(zeropage->boot_efi_info), sizeof(zeropage->boot_efi_info),
				&(mbi->mi_efi_info), sizeof(mbi->mi_efi_info));
	}
#endif
	/* copy part of the header into the zero page */
	hva = (struct zero_page *)gpa2hva(vm, (uint64_t)sw_kernel->kernel_load_addr);
	(void)memcpy_s(&(zeropage->hdr), sizeof(zeropage->hdr),
				&(hva->hdr), sizeof(hva->hdr));

	/* See if kernel has a RAM disk */
	if (ramdisk_info->src_addr != NULL) {
		/* Copy ramdisk load_addr and size in zeropage header structure
		 */
		addr = (uint64_t)ramdisk_info->load_addr;
		zeropage->hdr.ramdisk_addr = (uint32_t)addr;
		zeropage->hdr.ramdisk_size = (uint32_t)ramdisk_info->size;
	}

	/* Copy bootargs load_addr in zeropage header structure */
	addr = (uint64_t)bootargs_info->load_addr;
	zeropage->hdr.bootargs_addr = (uint32_t)addr;

	/* set constant arguments in zero page */
	zeropage->hdr.loader_type = 0xffU;
	zeropage->hdr.load_flags |= (1U << 5U);	/* quiet */

	/* Create/add e820 table entries in zeropage */
	zeropage->e820_nentries = (uint8_t)create_zeropage_e820(zeropage, vm);
	clac();

	/* Return Physical Base Address of zeropage */
	return gpa;
}

/**
 * @pre vm != NULL
 */
static void prepare_loading_bzimage(struct acrn_vm *vm, struct acrn_vcpu *vcpu)
{
	uint32_t i;
	char  dyn_bootargs[100] = {0};
	uint32_t kernel_entry_offset;
	struct zero_page *zeropage;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	const struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	/* calculate the kernel entry point */
	zeropage = (struct zero_page *)sw_kernel->kernel_src_addr;
	stac();
	kernel_entry_offset = (uint32_t)(zeropage->hdr.setup_sects + 1U) * 512U;
	clac();
	if (vcpu->arch.cpu_mode == CPU_MODE_64BIT) {
		/* 64bit entry is the 512bytes after the start */
		kernel_entry_offset += 512U;
	}

	sw_kernel->kernel_entry_addr = (void *)((uint64_t)sw_kernel->kernel_load_addr + kernel_entry_offset);

	/* Documentation states: ebx=0, edi=0, ebp=0, esi=ptr to
	 * zeropage
	 */
	for (i = 0U; i < NUM_GPRS; i++) {
		vcpu_set_gpreg(vcpu, i, 0UL);
	}

	/* add "hugepagesz=1G hugepages=x" to cmdline for 1G hugepage
	 * reserving. Current strategy is "total_mem_size in Giga -
	 * remained 1G pages" for reserving.
	 */
	if (is_sos_vm(vm) && (bootargs_info->load_addr != NULL)) {
		int64_t reserving_1g_pages;

		reserving_1g_pages = (vm_config->memory.size >> 30U) - NUM_REMAIN_1G_PAGES;
		if (reserving_1g_pages > 0) {
			snprintf(dyn_bootargs, 100U, " hugepagesz=1G hugepages=%ld", reserving_1g_pages);
			(void)copy_to_gpa(vm, dyn_bootargs, ((uint64_t)bootargs_info->load_addr
				+ bootargs_info->size), (strnlen_s(dyn_bootargs, 99U) + 1U));
		}
	}

	/* Create Zeropage and copy Physical Base Address of Zeropage
	 * in RSI
	 */
	vcpu_set_gpreg(vcpu, CPU_REG_RSI, create_zero_page(vm));
	pr_info("%s, RSI pointing to zero page for VM %d at GPA %X",
			__func__, vm->vm_id, vcpu_get_gpreg(vcpu, CPU_REG_RSI));
}

/**
 * @pre vm != NULL
 */
static void prepare_loading_rawimage(struct acrn_vm *vm)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	const struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

	sw_kernel->kernel_entry_addr = (void *)vm_config->os_config.kernel_entry_addr;
}

/**
 * @pre vm != NULL
 */
int32_t vm_sw_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	struct sw_module_info *ramdisk_info = &(vm->sw.ramdisk_info);
	struct sw_module_info *acpi_info = &(vm->sw.acpi_info);
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);

	pr_dbg("Loading guest to run-time location");

	/*
	 * TODO:
	 *    - We need to initialize the guest bsp registers according to
	 *      guest boot mode (real mode vs protect mode)
	 */
	init_vcpu_protect_mode_regs(vcpu, get_guest_gdt_base_gpa(vcpu->vm));

	/* Copy the guest kernel image to its run-time location */
	(void)copy_to_gpa(vm, sw_kernel->kernel_src_addr,
		(uint64_t)sw_kernel->kernel_load_addr, sw_kernel->kernel_size);

	/* Check if a RAM disk is present */
	if (ramdisk_info->size != 0U) {
		/* Copy RAM disk to its load location */
		(void)copy_to_gpa(vm, ramdisk_info->src_addr,
			(uint64_t)ramdisk_info->load_addr,
			ramdisk_info->size);
	}
	/* Copy Guest OS bootargs to its load location */
	if ((bootargs_info->size != 0U) && (bootargs_info->load_addr != NULL)) {
		(void)copy_to_gpa(vm, bootargs_info->src_addr,
			(uint64_t)bootargs_info->load_addr,
			(strnlen_s((char *)bootargs_info->src_addr, MAX_BOOTARGS_SIZE) + 1U));
	}
	/* Copy Guest OS ACPI to its load location */
	if (acpi_info->size == ACPI_MODULE_SIZE) {
		(void)copy_to_gpa(vm, acpi_info->src_addr, (uint64_t)acpi_info->load_addr, ACPI_MODULE_SIZE);
	}
	switch (vm->sw.kernel_type) {
	case KERNEL_BZIMAGE:
		prepare_loading_bzimage(vm, vcpu);
		break;
	case KERNEL_ZEPHYR:
		prepare_loading_rawimage(vm);
		break;
	default:
		pr_err("%s, Loading VM SW failed", __func__);
		ret = -EINVAL;
		break;
	}

	if (ret == 0) {
		/* Set VCPU entry point to kernel entry */
		vcpu_set_rip(vcpu, (uint64_t)sw_kernel->kernel_entry_addr);
		pr_info("%s, VM %hu VCPU %hu Entry: 0x%016lx ", __func__, vm->vm_id, vcpu->vcpu_id,
			sw_kernel->kernel_entry_addr);
	}

	return ret;
}
