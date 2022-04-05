/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/guest/vm.h>
#include <vboot.h>
#include <elf.h>
#include <logmsg.h>

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
				/* copy_to_gpa has it's stac/clac inside. So call stac again here. */
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

	p_elf_header32 = (struct elf32_hdr *)p_elf_img;
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
					p_prg_tbl_head32->p_paddr, p_prg_tbl_head32->p_memsz);
				/* copy_to_gpa has it's stac/clac inside. So call stac again here. */
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

int32_t elf_loader(struct acrn_vm *vm)
{
	uint64_t vgdt_gpa = 0x800;

	/*
	 * TODO:
	 *    - We need to initialize the guest BSP(boot strap processor) registers according to
	 *	guest boot mode (real mode vs protect mode)
	 *    - The memory layout usage is unclear, only GDT might be needed as its boot param.
	 *	currently we only support Zephyr which has no needs on cmdline/e820/efimmap/etc.
	 *	hardcode the vGDT GPA to 0x800 where is not used by Zephyr so far;
	 */
	init_vcpu_protect_mode_regs(vcpu_from_vid(vm, BSP_CPU_ID), vgdt_gpa);

	return load_elf(vm);
}
