/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/e820.h>
#include <asm/zeropage.h>
#include <asm/guest/ept.h>
#include <asm/mmu.h>
#include <boot.h>
#include <efi_mmap.h>
#include <errno.h>
#include <logmsg.h>

/* Define a 32KB memory block to store LaaG VM load params in guest address space
 * The params including:
 *	Init GDT entries : 1KB (must be 8byte aligned)
 *	Linux Zeropage : 4KB
 *	Boot cmdline : 2KB
 *	EFI memory map : 12KB
 *	Reserved region for trampoline code : 8KB
 * Each param should keep 8byte aligned and the total region size should be less than 32KB
 * so that it could be put below MEM_1M.
 * Please note in Linux VM, the last 8KB space below MEM_1M is for trampoline code. The block
 * should be able to accommodate it and so that avoid the trampoline corruption.
 */
#define BZIMG_LOAD_PARAMS_SIZE			(MEM_4K * 8)
#define BZIMG_INITGDT_GPA(load_params_gpa)	(load_params_gpa + 0UL)
#define BZIMG_ZEROPAGE_GPA(load_params_gpa)	(load_params_gpa + MEM_1K)
#define BZIMG_CMDLINE_GPA(load_params_gpa)	(load_params_gpa + MEM_1K + MEM_4K)
#define BZIMG_EFIMMAP_GPA(load_params_gpa)	(load_params_gpa + MEM_1K + MEM_4K + MEM_2K)

/**
 * @pre vm != NULL && efi_mmap_desc != NULL
 */
static uint16_t create_sos_vm_efi_mmap_desc(struct acrn_vm *vm, struct efi_memory_desc *efi_mmap_desc)
{
	uint16_t i, desc_idx = 0U;
	const struct efi_memory_desc *hv_efi_mmap_desc = get_efi_mmap_entry();

	for (i = 0U; i < get_efi_mmap_entries_count(); i++) {
		/* Below efi mmap desc types in native should be kept as original for SOS VM */
		if ((hv_efi_mmap_desc[i].type == EFI_RESERVED_MEMORYTYPE)
				|| (hv_efi_mmap_desc[i].type == EFI_UNUSABLE_MEMORY)
				|| (hv_efi_mmap_desc[i].type == EFI_ACPI_RECLAIM_MEMORY)
				|| (hv_efi_mmap_desc[i].type == EFI_ACPI_MEMORY_NVS)
				|| (hv_efi_mmap_desc[i].type == EFI_BOOT_SERVICES_CODE)
				|| (hv_efi_mmap_desc[i].type == EFI_BOOT_SERVICES_DATA)
				|| (hv_efi_mmap_desc[i].type == EFI_RUNTIME_SERVICES_CODE)
				|| (hv_efi_mmap_desc[i].type == EFI_RUNTIME_SERVICES_DATA)
				|| (hv_efi_mmap_desc[i].type == EFI_MEMORYMAPPED_IO)
				|| (hv_efi_mmap_desc[i].type == EFI_MEMORYMAPPED_IOPORTSPACE)
				|| (hv_efi_mmap_desc[i].type == EFI_PALCODE)
				|| (hv_efi_mmap_desc[i].type == EFI_PERSISTENT_MEMORY)) {

			efi_mmap_desc[desc_idx] = hv_efi_mmap_desc[i];
			desc_idx++;
		}
	}

	for (i = 0U; i < vm->e820_entry_num; i++) {
		/* The memory region with e820 type of RAM could be acted as EFI_CONVENTIONAL_MEMORY
		 * for SOS VM, the region which occupied by HV and pre-launched VM has been filtered
		 * already, so it is safe for SOS VM.
		 * As SOS VM start to run after efi call ExitBootService(), the type of EFI_LOADER_CODE
		 * and EFI_LOADER_DATA which have been mapped to E820_TYPE_RAM are not needed.
		 */
		if (vm->e820_entries[i].type == E820_TYPE_RAM) {
			efi_mmap_desc[desc_idx].type = EFI_CONVENTIONAL_MEMORY;
			efi_mmap_desc[desc_idx].phys_addr = vm->e820_entries[i].baseaddr;
			efi_mmap_desc[desc_idx].virt_addr = vm->e820_entries[i].baseaddr;
			efi_mmap_desc[desc_idx].num_pages = vm->e820_entries[i].length / PAGE_SIZE;
			efi_mmap_desc[desc_idx].attribute = EFI_MEMORY_WB;
			desc_idx++;
		}
	}

	for (i = 0U; i < desc_idx; i++) {
		pr_dbg("SOS VM efi mmap desc[%d]: addr: 0x%lx, len: 0x%lx, type: %d", i,
			efi_mmap_desc[i].phys_addr, efi_mmap_desc[i].num_pages * PAGE_SIZE, efi_mmap_desc[i].type);
	}

	return desc_idx;
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
static uint64_t create_zero_page(struct acrn_vm *vm, uint64_t load_params_gpa)
{
	struct zero_page *zeropage, *hva;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	struct sw_module_info *ramdisk_info = &(vm->sw.ramdisk_info);
	uint64_t gpa, addr;

	gpa = BZIMG_ZEROPAGE_GPA(load_params_gpa);
	hva = (struct zero_page *)gpa2hva(vm, gpa);
	zeropage = hva;

	stac();
	/* clear the zeropage */
	(void)memset(zeropage, 0U, MEM_2K);

#ifdef CONFIG_MULTIBOOT2
	if (is_sos_vm(vm)) {
		struct acrn_boot_info *abi = get_acrn_boot_info();

		if (boot_from_uefi(abi)) {
			struct efi_info *sos_efi_info = &zeropage->boot_efi_info;
			uint64_t efi_mmap_gpa = BZIMG_EFIMMAP_GPA(load_params_gpa);
			struct efi_memory_desc *efi_mmap_desc = (struct efi_memory_desc *)gpa2hva(vm, efi_mmap_gpa);
			uint16_t efi_mmap_desc_nr = create_sos_vm_efi_mmap_desc(vm, efi_mmap_desc);

			sos_efi_info->loader_signature = 0x34364c45; /* "EL64" */
			sos_efi_info->memdesc_version = abi->uefi_info.memdesc_version;
			sos_efi_info->memdesc_size = sizeof(struct efi_memory_desc);
			sos_efi_info->memmap_size = efi_mmap_desc_nr * sizeof(struct efi_memory_desc);
			sos_efi_info->memmap = (uint32_t)efi_mmap_gpa;
			sos_efi_info->memmap_hi = (uint32_t)(efi_mmap_gpa >> 32U);
			sos_efi_info->systab = abi->uefi_info.systab;
			sos_efi_info->systab_hi = abi->uefi_info.systab_hi;
		}
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
static void prepare_loading_bzimage(struct acrn_vm *vm, struct acrn_vcpu *vcpu, uint64_t load_params_gpa)
{
	uint32_t i;
	uint32_t kernel_entry_offset;
	struct zero_page *zeropage;
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);

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

	/* Create Zeropage and copy Physical Base Address of Zeropage
	 * in RSI
	 */
	vcpu_set_gpreg(vcpu, CPU_REG_RSI, create_zero_page(vm, load_params_gpa));
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
 * @pre sw_module != NULL
 */
static void load_sw_module(struct acrn_vm *vm, struct sw_module_info *sw_module)
{
	if (sw_module->size != 0) {
		(void)copy_to_gpa(vm, sw_module->src_addr, (uint64_t)sw_module->load_addr, sw_module->size);
	}
}

/**
 * @pre vm != NULL
 */
static void load_sw_modules(struct acrn_vm *vm, uint64_t load_params_gpa)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	struct sw_module_info *ramdisk_info = &(vm->sw.ramdisk_info);
	struct sw_module_info *acpi_info = &(vm->sw.acpi_info);

	pr_dbg("Loading guest to run-time location");

	/* Copy the guest kernel image to its run-time location */
	(void)copy_to_gpa(vm, sw_kernel->kernel_src_addr,
		(uint64_t)sw_kernel->kernel_load_addr, sw_kernel->kernel_size);

	if (vm->sw.kernel_type == KERNEL_BZIMAGE) {

		load_sw_module(vm, ramdisk_info);

		bootargs_info->load_addr = (void *)BZIMG_CMDLINE_GPA(load_params_gpa);

		load_sw_module(vm, bootargs_info);
	}

	/* Copy Guest OS ACPI to its load location */
	load_sw_module(vm, acpi_info);

}

static int32_t vm_bzimage_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);
	uint64_t load_params_gpa = find_space_from_ve820(vm, BZIMG_LOAD_PARAMS_SIZE, MEM_4K, MEM_1M);

	if (load_params_gpa != INVALID_GPA) {
		/* We boot bzImage from protected mode directly */
		init_vcpu_protect_mode_regs(vcpu, BZIMG_INITGDT_GPA(load_params_gpa));

		load_sw_modules(vm, load_params_gpa);

		prepare_loading_bzimage(vm, vcpu, load_params_gpa);
	} else {
		ret = -ENOMEM;
	}

	return ret;
}

static int32_t vm_rawimage_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	uint64_t load_params_gpa = 0x800;

	/*
	 * TODO:
	 *    - We need to initialize the guest bsp registers according to
	 *	guest boot mode (real mode vs protect mode)
	 *    - The memory layout usage is unclear, only GDT might be needed as its boot param.
	 *	currently we only support Zephyr which has no needs on cmdline/e820/efimmap/etc.
	 *	hardcode the vGDT GPA to 0x800 where is not used by Zephyr so far;
	 */
	init_vcpu_protect_mode_regs(vcpu_from_vid(vm, BSP_CPU_ID), load_params_gpa);

	load_sw_modules(vm, load_params_gpa);

	prepare_loading_rawimage(vm);

	return ret;
}

/**
 * @pre vm != NULL
 */
int32_t vm_sw_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);

	if (vm->sw.kernel_type == KERNEL_BZIMAGE) {

		ret = vm_bzimage_loader(vm);

	} else if (vm->sw.kernel_type == KERNEL_ZEPHYR){

		ret = vm_rawimage_loader(vm);

	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		/* Set VCPU entry point to kernel entry */
		vcpu_set_rip(vcpu, (uint64_t)vm->sw.kernel_info.kernel_entry_addr);
		pr_info("%s, VM %hu VCPU %hu Entry: 0x%016lx ", __func__, vm->vm_id, vcpu->vcpu_id,
			vm->sw.kernel_info.kernel_entry_addr);
	}

	return ret;
}
