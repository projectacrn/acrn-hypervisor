/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <asm/guest/ept.h>
#include <asm/mmu.h>
#include <vboot.h>
#include <elf.h>
#include <logmsg.h>
#include <vacpi.h>

/* Define a memory block to store ELF format VM load params in guest address space
 * The params including:
 *	MISC info: 1KB
 *		including: Init GDT(40 bytes),ACRN ELF loader name(20 bytes), ACPI RSDP table(36 bytes).
 *	Multiboot info : 4KB
 *	Boot cmdline : 2KB
 *	memory map : 20KB (enough to put memory entries for multiboot 0.6.96 or multiboot 2.0)
 * Each param should keep 8byte aligned and the total region should be able to put below MEM_1M.
 * The total params size is:
 * (MEM_1K + MEM_4K + MEM_2K + 20K) = 27KB
 */

struct elf_boot_para {
	char init_gdt[40];
	char loader_name[20];
	struct acpi_table_rsdp rsdp;
	struct multiboot_info mb_info;
	char cmdline[MEM_2K];
	char mmap[MEM_4K * 5U];
} __aligned(8);

int32_t prepare_elf_cmdline(struct acrn_vm *vm, uint64_t param_cmd_gpa)
{
	return copy_to_gpa(vm, vm->sw.bootargs_info.src_addr, param_cmd_gpa,
		           vm->sw.bootargs_info.size);
}

uint32_t prepare_multiboot_mmap(struct acrn_vm *vm, uint64_t param_mmap_gpa)
{
	uint32_t i, mmap_length = 0U;
	struct multiboot_mmap mmap_entry;
	uint64_t mmap_gpa = param_mmap_gpa;

	for (i = 0U; i < vm->e820_entry_num; i++) {
		mmap_entry.size = 20U;
		mmap_entry.baseaddr = vm->e820_entries[i].baseaddr;
		mmap_entry.length = vm->e820_entries[i].length;
		mmap_entry.type = vm->e820_entries[i].type;
		if (mmap_entry.type > MULTIBOOT_MEMORY_BADRAM) {
			mmap_entry.type = MULTIBOOT_MEMORY_RESERVED;
		}

		if (copy_to_gpa(vm, &mmap_entry, mmap_gpa,
			sizeof(struct multiboot_mmap)) != 0U) {
			mmap_length = 0U;
			break;
		}
		mmap_gpa += sizeof(struct multiboot_mmap);
		mmap_length += sizeof(struct multiboot_mmap);
	}

	return mmap_length;
}

uint32_t prepare_loader_name(struct acrn_vm *vm, uint64_t param_ldrname_gpa)
{
	char loader_name[MAX_LOADER_NAME_SIZE] = "ACRN ELF LOADER";

	return (copy_to_gpa(vm, (void *)loader_name, param_ldrname_gpa,
		MAX_LOADER_NAME_SIZE));
}

/**
 * @pre vm != NULL
 * must run in stac/clac context
 */
static void *do_load_elf64(struct acrn_vm *vm)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	void *p_elf_img = (void *)sw_kernel->kernel_src_addr;
	struct elf64_hdr *p_elf_header64 = (struct elf64_hdr *)p_elf_img;
	struct elf64_prog_entry *p_prg_tbl_head64;
	struct elf64_sec_entry *p_sec_tbl_head64, *p_shstr_tbl_head64;
	const char *p_shstr_tbl, *p_sec_name;
	void *elf_entry = NULL, *p_elf_bss = NULL;
	uint32_t i;

	/* Currently only ET_EXEC is supported */
	if (p_elf_header64->e_type == ET_EXEC) {
		p_prg_tbl_head64 = (struct elf64_prog_entry *)(p_elf_img + p_elf_header64->e_phoff);
		/* Prepare program entries */
		for (i = 0U; i < p_elf_header64->e_phnum; i++) {
			/**
			 * We now only support PT_LOAD type. It needs to copy from file to ram
			 * TODO: More program types may be needed here
			 */
			if (p_prg_tbl_head64->p_type == PT_LOAD) {
				/**
				 * copy_to_gpa will check whether the gpa is in EPT, and print message
				 * if anything wrong.
				 * However, the guest OS may still fail to boot if they load segments
				 * to invalid gpa such as ACPI area defined in ve820.
				 *
				 * We assume that the guest elf can put segments to valid gpa.
				 */
				(void)copy_to_gpa(vm, p_elf_img + p_prg_tbl_head64->p_offset,
					p_prg_tbl_head64->p_paddr, (uint32_t)p_prg_tbl_head64->p_filesz);
				/* copy_to_gpa has its own stac/clac inside. Call stac again here to keep
				 * the context. */
				stac();
			}
			p_prg_tbl_head64++;
		}

		/* Find and clear bss sections */
		p_sec_tbl_head64 = (struct elf64_sec_entry *)(p_elf_img + p_elf_header64->e_shoff);
		p_shstr_tbl_head64 = p_sec_tbl_head64 + p_elf_header64->e_shstrndx;
		p_shstr_tbl = (char *)(p_elf_img + p_shstr_tbl_head64->sh_offset);
		/* Currently we don't support relocatable sections(sh_type is SHT_REL or SHT_RELA).
			Assume that the guest elf do not have relocatable sections. */
		for (i = 0U; i < p_elf_header64->e_shnum; i++) {
			/* A section entry's name is an offset, real string is in string tab */
			p_sec_name = p_shstr_tbl + p_sec_tbl_head64->sh_name;
			if ((strncmp(p_sec_name, "bss", 3) == 0) || (strncmp(p_sec_name, ".bss", 4) == 0)) {
				p_elf_bss = gpa2hva(vm, p_sec_tbl_head64->sh_addr);
				memset(p_elf_bss, 0U, p_sec_tbl_head64->sh_size);
			}
			p_sec_tbl_head64++;
		}

		elf_entry = (void *)p_elf_header64->e_entry;
	} else {
		pr_err("%s, elf type(%x) not supported!", __func__, p_elf_header64->e_type);
	}
	/* For 64bit elf, entry address above 4G is not currently supported. Assume that it's below 4G. */
	return elf_entry;
}

/**
 * @pre vm != NULL
 * must run in stac/clac context
 */
static void *do_load_elf32(struct acrn_vm *vm)
{
	struct sw_kernel_info *sw_kernel = &(vm->sw.kernel_info);
	void *p_elf_img = (void *)sw_kernel->kernel_src_addr;
	struct elf32_hdr *p_elf_header32 = (struct elf32_hdr *)p_elf_img;
	struct elf32_prog_entry *p_prg_tbl_head32;
	struct elf32_sec_entry *p_sec_tbl_head32, *p_shstr_tbl_head32;
	const char *p_shstr_tbl, *p_sec_name;
	void *elf_entry = NULL, *p_elf_bss = NULL;
	uint32_t i;

	/* Currently only ET_EXEC is supported */
	if (p_elf_header32->e_type == ET_EXEC) {
		p_prg_tbl_head32 = (struct elf32_prog_entry *)(p_elf_img + p_elf_header32->e_phoff);
		/* Copy program entries */
		for (i = 0U; i < p_elf_header32->e_phnum; i++) {
			/**
			 * We now only support PT_LOAD type. It needs to copy from file to ram
			 * TODO: More program types may be needed here
			 */
			if (p_prg_tbl_head32->p_type == PT_LOAD) {
				/**
				 * copy_to_gpa will check whether the gpa is in EPT, and print message
				 * if anything wrong.
				 * However, the guest OS may still fail to boot if they load segments
				 * to invalid gpa such as ACPI area defined in ve820.
				 *
				 * We assume that the guest elf can put segments to valid gpa.
				 */
				(void)copy_to_gpa(vm, p_elf_img + p_prg_tbl_head32->p_offset,
					p_prg_tbl_head32->p_paddr, p_prg_tbl_head32->p_filesz);
				/* copy_to_gpa has its own stac/clac inside. Call stac again here to keep
				 * the context. */
				stac();
			}
			p_prg_tbl_head32++;
		}

		/* Find and clear bss sections */
		p_sec_tbl_head32 = (struct elf32_sec_entry *)(p_elf_img + p_elf_header32->e_shoff);
		p_shstr_tbl_head32 = p_sec_tbl_head32 + p_elf_header32->e_shstrndx;
		p_shstr_tbl = (char *)(p_elf_img + p_shstr_tbl_head32->sh_offset);
		/* Currently we don't support relocatable sections(sh_type is SHT_REL or SHT_RELA).
			Assume that the guest elf do not have relocatable sections. */
		for (i = 0U; i < p_elf_header32->e_shnum; i++) {
			/* A section entry's name is an offset, real string is in string tab */
			p_sec_name = p_shstr_tbl + p_sec_tbl_head32->sh_name;
			if ((strncmp(p_sec_name, "bss", 3) == 0) || (strncmp(p_sec_name, ".bss", 4) == 0)) {
				p_elf_bss = gpa2hva(vm, p_sec_tbl_head32->sh_addr);
				memset(p_elf_bss, 0U, p_sec_tbl_head32->sh_size);
			}
			p_sec_tbl_head32++;
		}

		elf_entry = (void *)(uint64_t)p_elf_header32->e_entry;
	} else {
		pr_err("%s, elf type(%x) not supported!", __func__, p_elf_header32->e_type);
	}

	return elf_entry;
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

struct multiboot_header *find_img_multiboot_header(struct acrn_vm *vm)
{
	uint16_t i, j;
	struct multiboot_header *ret = NULL;
	uint32_t *p = (uint32_t *)vm->sw.kernel_info.kernel_src_addr;

	/* Scan the first 8k to detect whether the elf needs multboot info prepared. */
	for (i = 0U; i <= (((MEM_4K * 2U) / sizeof(uint32_t)) - 3U); i++) {
		if (p[i] == MULTIBOOT_HEADER_MAGIC) {
			uint32_t sum = 0U;

			/* According to multiboot spec 0.6.96 sec 3.1.2.
			 * There are three u32:
			 *  offset   field
			 *    0      multiboot_header_magic
			 *    4      flags
			 *    8      checksum
			 * The sum of these three u32 should be u32 zero.
			 */
			for (j = 0U; j < 3U; j++) {
				sum += p[j + i];
			}

			if (0U == sum) {
				ret = (struct multiboot_header *)(p + i);
				break;
			}
		}
	}
	return ret;
}

int32_t elf_loader(struct acrn_vm *vm)
{
	int32_t ret = 0;
	struct multiboot_header *mb_hdr;
	/* Get primary vcpu */
	struct acrn_vcpu *vcpu = vcpu_from_vid(vm, BSP_CPU_ID);
	/*
	 * Assuming the guest elf would not load content to GPA space under
	 * VIRT_RSDP_ADDR, and guest gpa load space is sure under address
	 * we prepared in ve820.c. In the future, need to check each
	 * ELF load entry according to ve820 if relocation is not supported.
	 */
	uint64_t load_params_gpa = find_space_from_ve820(vm, sizeof(struct elf_boot_para),
				   MEM_4K, VIRT_RSDP_ADDR);

	if (load_params_gpa != INVALID_GPA) {
		/* We boot ELF Image from protected mode directly */
		init_vcpu_protect_mode_regs(vcpu, load_params_gpa +
					    offsetof(struct elf_boot_para, init_gdt));
		stac();
		mb_hdr = find_img_multiboot_header(vm);
		clac();
		if (mb_hdr != NULL) {
			uint32_t mmap_length = 0U;
			struct multiboot_info mb_info;

			stac();
			if ((mb_hdr->flags & MULTIBOOT_HEADER_NEED_MEMINFO) != 0U) {
				mmap_length = prepare_multiboot_mmap(vm, load_params_gpa +
						offsetof(struct elf_boot_para, mmap));
			}

			if (mmap_length != 0U) {
				mb_info.mi_flags |= MULTIBOOT_INFO_HAS_MMAP;
				mb_info.mi_mmap_addr = (uint32_t)(load_params_gpa +
						offsetof(struct elf_boot_para, mmap));
				mb_info.mi_mmap_length = mmap_length;
			}
			ret = prepare_elf_cmdline(vm, load_params_gpa +
						offsetof(struct elf_boot_para, cmdline));
			if (ret == 0) {
				mb_info.mi_flags |= MULTIBOOT_INFO_HAS_CMDLINE;
				mb_info.mi_cmdline = load_params_gpa +
						offsetof(struct elf_boot_para, cmdline);
				ret = prepare_loader_name(vm, load_params_gpa +
						offsetof(struct elf_boot_para, loader_name));
			}

			if (ret == 0) {
				mb_info.mi_flags |= MULTIBOOT_INFO_HAS_LOADER_NAME;
				mb_info.mi_loader_name = load_params_gpa +
						offsetof(struct elf_boot_para, loader_name);
				ret = copy_to_gpa(vm, (void *)&mb_info, load_params_gpa +
						offsetof(struct elf_boot_para, mb_info),
						sizeof(struct multiboot_info));
			}

			if (ret == 0) {
				vcpu_set_gpreg(vcpu, CPU_REG_RAX, MULTIBOOT_INFO_MAGIC);
				vcpu_set_gpreg(vcpu, CPU_REG_RBX, load_params_gpa +
						offsetof(struct elf_boot_para, mb_info));
				/* other vcpu regs should have satisfied multiboot requirement already. */
			}
			clac();
		}
		/*
		 * elf_loader need support non-multiboot header image
		 * at the same time.
		 */
		if (ret == 0) {
			ret = load_elf(vm);
		}
	}
	return ret;
}
