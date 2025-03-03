/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/e820.h>
#include <asm/zeropage.h>
#include <asm/guest/ept.h>
#include <asm/mmu.h>
#include <boot.h>
#include <vboot.h>
#include <vacpi.h>
#include <efi_mmap.h>
#include <errno.h>
#include <logmsg.h>

#define DBG_LEVEL_VM_BZIMAGE	6U

/* Define a memory block to store LaaG VM load params in guest address space
 * The params including:
 *	Init GDT entries : 1KB (must be 8byte aligned)
 *	Linux Zeropage : 4KB
 *	Boot cmdline : 2KB
 *	EFI memory map : MAX_EFI_MMAP_ENTRIES * sizeof(struct efi_memory_desc)
 *	Reserved region for trampoline code : 8KB
 * Each param should keep 8byte aligned and the total region should be able to put below MEM_1M.
 * Please note in Linux VM, the last 8KB space below MEM_1M is for trampoline code. The block
 * should be able to accommodate it so that avoid the trampoline corruption. So the params size is:
 * (MEM_1K + MEM_4K + MEM_2K + 40B * MAX_EFI_MMAP_ENTRIES + MEM_8K)
 */
#define BZIMG_LOAD_PARAMS_SIZE			(MEM_1K * 15U + MAX_EFI_MMAP_ENTRIES * sizeof(struct efi_memory_desc))
#define BZIMG_INITGDT_GPA(load_params_gpa)	((load_params_gpa) + 0UL)
#define BZIMG_ZEROPAGE_GPA(load_params_gpa)	((load_params_gpa) + MEM_1K)
#define BZIMG_CMDLINE_GPA(load_params_gpa)	((load_params_gpa) + MEM_1K + MEM_4K)
#define BZIMG_EFIMMAP_GPA(load_params_gpa)	((load_params_gpa) + MEM_1K + MEM_4K + MEM_2K)

/* TODO:
 * The value is referenced from Linux boot protocal for old kernels,
 * but this should be configurable for different OS. */
#define DEFAULT_RAMDISK_GPA_MAX		0x37ffffffUL

#define PRE_VM_MAX_RAM_ADDR_BELOW_4GB		(VIRT_ACPI_DATA_ADDR - 1UL)

static void *get_initrd_load_addr(struct acrn_vm *vm, uint64_t kernel_start)
{
	uint64_t ramdisk_load_gpa = INVALID_GPA;
	uint64_t ramdisk_gpa_max = DEFAULT_RAMDISK_GPA_MAX;
	struct zero_page *zeropage = (struct zero_page *)vm->sw.kernel_info.kernel_src_addr;
	uint32_t kernel_init_size, kernel_align, initrd_addr_max;
	uint64_t kernel_end;

	/* Per Linux boot protocol, the Kernel need a size of contiguous
	 * memory(i.e. init_size field in zeropage) from its extract address to boot,
	 * and initrd_addr_max field specifies the maximum address of the ramdisk.
	 * Per kernel src head_64.S, decompressed kernel start at 2M aligned to the
	 * compressed kernel load address.
	 */
	stac();
	kernel_init_size = zeropage->hdr.init_size;
	kernel_align = zeropage->hdr.kernel_alignment;
	initrd_addr_max = zeropage->hdr.initrd_addr_max;
	clac();
	kernel_end = roundup(kernel_start, kernel_align) + kernel_init_size;

	if (initrd_addr_max != 0U) {
		ramdisk_gpa_max = initrd_addr_max;
	}

	if (is_service_vm(vm)) {
		uint64_t mods_start, mods_end;

		get_boot_mods_range(&mods_start, &mods_end);
		mods_start = service_vm_hpa2gpa(mods_start);
		mods_end = service_vm_hpa2gpa(mods_end);

		if (vm->sw.ramdisk_info.src_addr != NULL) {
			ramdisk_load_gpa = service_vm_hpa2gpa((uint64_t)vm->sw.ramdisk_info.src_addr);
		}

		/* For Service VM, the ramdisk has been loaded by bootloader, so in most cases
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
	dev_dbg(DBG_LEVEL_VM_BZIMAGE, "VM%d ramdisk load_addr: 0x%lx", vm->vm_id, ramdisk_load_gpa);

	return (ramdisk_load_gpa == INVALID_GPA) ? NULL : (void *)ramdisk_load_gpa;
}

/**
 * @pre vm != NULL
 */
static void *get_bzimage_kernel_load_addr(struct acrn_vm *vm)
{
	void *load_addr = NULL;
	struct vm_sw_info *sw_info = &vm->sw;
	struct zero_page *zeropage;

	/* According to the explaination for pref_address
	 * in Documentation/x86/boot.txt, a relocating
	 * bootloader should attempt to load kernel at pref_address
	 * if possible. A non-relocatable kernel will unconditionally
	 * move itself and to run at this address.
	 */
	zeropage = (struct zero_page *)sw_info->kernel_info.kernel_src_addr;

	stac();
	if ((is_service_vm(vm)) && (zeropage->hdr.relocatable_kernel != 0U)) {
		uint64_t mods_start, mods_end;
		uint64_t kernel_load_gpa = INVALID_GPA;
		uint32_t kernel_align = zeropage->hdr.kernel_alignment;
		uint32_t kernel_init_size = zeropage->hdr.init_size;
		/* Because the kernel load address need to be up aligned to kernel_align size
		 * whereas find_space_from_ve820() can only return page aligned address,
		 * we enlarge the needed size to (kernel_init_size + kernel_align).
		 */
		uint32_t kernel_size = kernel_init_size + kernel_align;

		get_boot_mods_range(&mods_start, &mods_end);
		mods_start = service_vm_hpa2gpa(mods_start);
		mods_end = service_vm_hpa2gpa(mods_end);

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
		if (is_service_vm(vm)) {
			/* The non-relocatable Servic VM kernel might overlap with boot modules. */
			pr_err("Non-relocatable kernel found, risk to boot!");
		}
	}
	clac();

	if (load_addr == NULL) {
		pr_err("Could not get kernel load addr of VM %d .", vm->vm_id);
	}

	dev_dbg(DBG_LEVEL_VM_BZIMAGE, "VM%d kernel load_addr: 0x%lx", vm->vm_id, load_addr);
	return load_addr;
}

#ifdef CONFIG_MULTIBOOT2
/**
 * @pre vm != NULL && efi_mmap_desc != NULL
 */
static uint16_t create_service_vm_efi_mmap_desc(struct acrn_vm *vm, struct efi_memory_desc *efi_mmap_desc)
{
	uint16_t i, desc_idx = 0U;
	const struct efi_memory_desc *hv_efi_mmap_desc = get_efi_mmap_entry();

	for (i = 0U; i < (uint16_t)get_efi_mmap_entries_count(); i++) {
		/* Below efi mmap desc types in native should be kept as original for Service VM */
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

	for (i = 0U; i < (uint16_t)vm->e820_entry_num; i++) {
		/* The memory region with e820 type of RAM could be acted as EFI_CONVENTIONAL_MEMORY
		 * for Service VM, the region which occupied by HV and pre-launched VM has been filtered
		 * already, so it is safe for Service VM.
		 * As Service VM start to run after efi call ExitBootService(), the type of EFI_LOADER_CODE
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
		pr_dbg("Service VM efi mmap desc[%d]: addr: 0x%lx, len: 0x%lx, type: %d", i,
			efi_mmap_desc[i].phys_addr, efi_mmap_desc[i].num_pages * PAGE_SIZE, efi_mmap_desc[i].type);
	}

	return desc_idx;
}
#endif

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
	(void)memset(zeropage, 0U, MEM_4K);

#ifdef CONFIG_MULTIBOOT2
	if (is_service_vm(vm)) {
		struct acrn_boot_info *abi = get_acrn_boot_info();

		if (boot_from_uefi(abi)) {
			struct efi_info *service_vm_efi_info = &zeropage->boot_efi_info;
			uint64_t efi_mmap_gpa = BZIMG_EFIMMAP_GPA(load_params_gpa);
			struct efi_memory_desc *efi_mmap_desc = (struct efi_memory_desc *)gpa2hva(vm, efi_mmap_gpa);
			uint16_t efi_mmap_desc_nr = create_service_vm_efi_mmap_desc(vm, efi_mmap_desc);

			service_vm_efi_info->loader_signature = 0x34364c45; /* "EL64" */
			service_vm_efi_info->memdesc_version = abi->uefi_info.memdesc_version;
			service_vm_efi_info->memdesc_size = sizeof(struct efi_memory_desc);
			service_vm_efi_info->memmap_size = efi_mmap_desc_nr * sizeof(struct efi_memory_desc);
			service_vm_efi_info->memmap = (uint32_t)efi_mmap_gpa;
			service_vm_efi_info->memmap_hi = (uint32_t)(efi_mmap_gpa >> 32U);
			service_vm_efi_info->systab = abi->uefi_info.systab;
			service_vm_efi_info->systab_hi = abi->uefi_info.systab_hi;
		}
	}
#endif
	/* copy part of the header into the zero page */
	hva = (struct zero_page *)sw_kernel->kernel_src_addr;
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
static void load_bzimage(struct acrn_vm *vm, struct acrn_vcpu *vcpu,
						uint64_t load_params_gpa, uint64_t kernel_load_gpa)
{
	uint32_t i;
	uint32_t prot_code_offset, prot_code_size, kernel_entry_offset;
	uint8_t setup_sectors;
	const struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	struct sw_module_info *bootargs_info = &(vm->sw.bootargs_info);
	struct sw_module_info *ramdisk_info = &(vm->sw.ramdisk_info);
	struct zero_page *zeropage = (struct zero_page *)sw_kernel->kernel_src_addr;

	/* The bzImage file consists of three parts:
	 * boot_params(i.e. zero page) + real mode setup code + compressed protected mode code
	 * The compressed proteced mode code start at offset (setup_sectors + 1U) * 512U of bzImage.
	 * Only protected mode code need to be loaded.
	 */
	stac();
	setup_sectors = (zeropage->hdr.setup_sects == 0U) ? 4U : zeropage->hdr.setup_sects;
	clac();
	prot_code_offset = (uint32_t)(setup_sectors + 1U) * 512U;
	prot_code_size = (sw_kernel->kernel_size > prot_code_offset) ?
				(sw_kernel->kernel_size - prot_code_offset) : 0U;

	/* Copy the protected mode part kernel code to its run-time location */
	(void)copy_to_gpa(vm, (sw_kernel->kernel_src_addr + prot_code_offset), kernel_load_gpa, prot_code_size);

	if (vm->sw.ramdisk_info.size > 0U) {
		/* Use customer specified ramdisk load addr if it is configured in VM configuration,
		 * otherwise use allocated address calculated by HV.
		 */
		if (vm_config->os_config.kernel_ramdisk_addr != 0UL) {
			vm->sw.ramdisk_info.load_addr = (void *)vm_config->os_config.kernel_ramdisk_addr;
		} else {
			vm->sw.ramdisk_info.load_addr = (void *)get_initrd_load_addr(vm, kernel_load_gpa);
			if (vm->sw.ramdisk_info.load_addr == NULL) {
				pr_err("failed to load initrd for VM%d !", vm->vm_id);
			}
		}

		/* Don't need to load ramdisk if src_addr and load_addr are pointed to same place. */
		if (gpa2hva(vm, (uint64_t)ramdisk_info->load_addr) != ramdisk_info->src_addr) {
			load_sw_module(vm, ramdisk_info);
		}
	}

	bootargs_info->load_addr = (void *)BZIMG_CMDLINE_GPA(load_params_gpa);

	load_sw_module(vm, bootargs_info);

	/* 32bit kernel entry is at where protected mode code loaded */
	kernel_entry_offset = 0U;
	if (vcpu->arch.cpu_mode == CPU_MODE_64BIT) {
		/* 64bit entry is the 512bytes after the start */
		kernel_entry_offset += 512U;
	}

	sw_kernel->kernel_entry_addr = (void *)(kernel_load_gpa + kernel_entry_offset);

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

int32_t bzimage_loader(struct acrn_vm *vm)
{
	int32_t ret = -ENOMEM;
	/* get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);
	uint64_t load_params_gpa = find_space_from_ve820(vm, BZIMG_LOAD_PARAMS_SIZE, MEM_4K, MEM_1M);

	if (load_params_gpa != INVALID_GPA) {
		uint64_t kernel_load_gpa = (uint64_t)get_bzimage_kernel_load_addr(vm);

		if (kernel_load_gpa != 0UL) {
			/* We boot bzImage from protected mode directly */
			init_vcpu_protect_mode_regs(vcpu, BZIMG_INITGDT_GPA(load_params_gpa));

			load_bzimage(vm, vcpu, load_params_gpa, kernel_load_gpa);

			ret = 0;
		}
	}

	return ret;
}
