/*-
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* This is a very simple elf binary loader. It only support static elf32 binary
 * loading. So we don't need to handle elf relocation. Just need to load the
 * PT_LOAD section to correct memory address and set correct entry.
 *
 * It also prepare simple multiboot info (only memory info) to guest. If some
 * other things are necessary for guest, we could add it per requirement.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <elf.h>

#include "types.h"
#include "dm.h"
#include "vmmapi.h"
#include "sw_load.h"
#include "acpi.h"

#define	ELF_BUF_LEN			(1024UL*8UL)
#define	MULTIBOOT_HEAD_MAGIC		(0x1BADB002U)
#define	MULTIBOOT_MACHINE_STATE_MAGIC	(0x2BADB002U)
/* The max length for GDT is 8192 * 8 bytes */
#define	GDT_LOAD_OFF(ctx)		(ctx->lowmem - 64U * KB)

static char elf_path[STR_LEN];
/* Whether we need to setup multiboot info for guest */
static int multiboot_image = 0;

/* Multiboot info. Compatible with Multiboot spec 0.6.96 with vesa removed */
#define	MULTIBOOT_INFO_MEMORY		(0x00000001U)
#define	MULTIBOOT_INFO_BOOTDEV		(0x00000002U)
#define	MULTIBOOT_INFO_CMDLINE		(0x00000004U)
#define	MULTIBOOT_INFO_MODS		(0x00000008U)
#define	MULTIBOOT_INFO_AOUT_SYMS	(0x00000010U)
#define	MULTIBOOT_INFO_ELF_SHDR		(0x00000020U)
#define	MULTIBOOT_INFO_MEM_MAP		(0x00000040U)
#define	MULTIBOOT_INFO_DRIVE_INFO	(0x00000080U)
#define	MULTIBOOT_INFO_CONFIG_TABLE	(0x00000100U)
#define	MULTIBOOT_INFO_BOOT_LOADER_NAME	(0x00000200U)
#define	MULTIBOOT_INFO_BOOT_APM_TABLE	(0x00000400U)

struct multiboot_info {
	uint32_t	flags;
	uint32_t	mem_lower;
	uint32_t	mem_upper;

	uint32_t	boot_device;
	uint32_t	cmdline;
	uint32_t	mods_count;
	uint32_t	mods_addr;

	uint32_t	bin_sec[4];

	uint32_t	mmap_length;
	uint32_t	mmap_addr;

	uint32_t	drives_length;
	uint32_t	drives_addr;

	uint32_t	config_table;

	uint32_t	boot_loader_name;
};

int
acrn_parse_elf(char *arg)
{
	int err = -1;
	size_t len = strnlen(arg, STR_LEN);
	size_t elfsz;

	if (len < STR_LEN) {
		strncpy(elf_path, arg, len + 1);
		if (check_image(elf_path, 0, &elfsz) == 0) {
			elf_file_name = elf_path;
			printf("SW_LOAD: get elf path %s\n", elf_path);
			err = 0;
		}
	}
	return err;
}

static int load_elf32(struct vmctx *ctx, FILE *fp, void *buf)
{
	int i;
	size_t phd_size, read_len;
	Elf32_Ehdr *elf32_header = (Elf32_Ehdr *)buf;
	Elf32_Phdr *elf32_phdr, *elf32_phdr_bk;

	phd_size = elf32_header->e_phentsize * elf32_header->e_phnum;
	elf32_phdr_bk = elf32_phdr = (Elf32_Phdr *)calloc(1, phd_size);
	if (elf32_phdr == NULL) {
		fprintf(stderr, "Can't allocate memory for elf program header\n");
		return -1;
	}

	fseek(fp, elf32_header->e_phoff, SEEK_SET);
	read_len = fread((void *)elf32_phdr, 1, phd_size, fp);
	if (read_len != phd_size) {
		fprintf(stderr, "can't get %ld data from elf file\n", phd_size);
	}

	for (i = 0; i < elf32_header->e_phnum; i++) {
		if (elf32_phdr->p_type == PT_LOAD) {
			if ((elf32_phdr->p_vaddr + elf32_phdr->p_memsz) >
					ctx->lowmem) {
				fprintf(stderr,
					"No enough memory to load elf file\n");
				free(elf32_phdr_bk);
				return -1;
			}

			void *seg_ptr = ctx->baseaddr + elf32_phdr->p_vaddr;

			/* Clear the segment memory in memory.
			 * This is required for BSS section
			 */
			memset(seg_ptr, 0, elf32_phdr->p_memsz);
			fseek(fp, elf32_phdr->p_offset, SEEK_SET);
			read_len = fread(seg_ptr, 1, elf32_phdr->p_filesz, fp);
			if (read_len != elf32_phdr->p_filesz) {
				fprintf(stderr, "Can't get %d data\n",
						elf32_phdr->p_filesz);
			}
		}

		elf32_phdr++;
	}

	free(elf32_phdr_bk);
	return 0;
}

static int
acrn_load_elf(struct vmctx *ctx, char *elf_file_name, unsigned long *entry,
		uint32_t *multiboot_flags)
{
	int i, ret = 0;
	FILE *fp;
	size_t read_len = 0;
	unsigned int *ptr32;
	char *elf_buf;
	Elf32_Ehdr *elf_ehdr;


	elf_buf = calloc(1, ELF_BUF_LEN);
	if (elf_buf == NULL) {
		fprintf(stderr, "Can't allocate elf buf\r\n");
		return -1;
	}

	fp = fopen(elf_file_name, "r");
	if (fp == NULL) {
		fprintf(stderr, "Can't open elf file: %s\r\n", elf_file_name);
		free(elf_buf);
		return -1;
	}

	read_len = fread(elf_buf, 1, ELF_BUF_LEN, fp);
	if (read_len != ELF_BUF_LEN) {
		fprintf(stderr, "Can't get %ld data from elf file\n",
				ELF_BUF_LEN);
	}

	/* Scan the first 8k to detect whether the elf needs multboot
	 * info prepared.
	 */
	ptr32 = (unsigned int *) elf_buf;
	for (i = 0; i <= ((ELF_BUF_LEN/4) - 3); i++) {
		if (ptr32[i] == MULTIBOOT_HEAD_MAGIC) {
			int j = 0;
			unsigned int sum = 0;

			/* According to multiboot spec 0.6.96 sec 3.1.2.
			 * There are three u32:
			 *  offset   field
			 *    0      multiboot_head_magic
			 *    4      flags
			 *    8      checksum
			 * The sum of these three u32 should be u32 zero.
			 */
			for (j = 0; j < 3; j++) {
				sum += ptr32[j + i];
			}

			if (0 == sum) {
				multiboot_image = 1;
				*multiboot_flags = ptr32[i + 1];
			}
			break;
		}
	}

	elf_ehdr = (Elf32_Ehdr *) elf_buf;

	if ((elf_ehdr->e_ident[EI_MAG0] != ELFMAG0) ||
		(elf_ehdr->e_ident[EI_MAG1] != ELFMAG1) ||
		(elf_ehdr->e_ident[EI_MAG2] != ELFMAG2) ||
		(elf_ehdr->e_ident[EI_MAG3] != ELFMAG3)) {
		fprintf(stderr, "This is not elf file\n");
		fclose(fp);
		free(elf_buf);

		return -1;
	}

	if (elf_ehdr->e_ident[EI_CLASS] == ELFCLASS32) {
		ret = load_elf32(ctx, fp, elf_buf);
	} else {
		fprintf(stderr, "No available 64bit elf loader ready yet\n");
		fclose(fp);
		free(elf_buf);
		return -1;
	}

	*entry = elf_ehdr->e_entry;
	fclose(fp);
	free(elf_buf);

	return ret;
}

/* Where we put multboot info to */
#define	MULTIBOOT_OFFSET	(0x20000)
static const uint64_t acrn_init_gdt[] = {
	0x0UL,
	0x00CF9B000000FFFFUL,	/* Linear Code */
	0x00CF93000000FFFFUL,	/* Linear Data */
};

int
acrn_sw_load_elf(struct vmctx *ctx)
{
	int ret;
	uint32_t multiboot_flags = 0;
	uint64_t entry = 0;
	struct multiboot_info *mi;

	ret = acrn_load_elf(ctx, elf_file_name, &entry, &multiboot_flags);
	if (ret < 0)
		return ret;

	/* set guest bsp state. Will call hypercall set bsp state
	 * after bsp is created.
	 */
	memset(&ctx->bsp_regs, 0, sizeof( struct acrn_set_vcpu_regs));
	ctx->bsp_regs.vcpu_id = 0;

	memcpy(ctx->baseaddr + GDT_LOAD_OFF(ctx), &acrn_init_gdt,
			sizeof(acrn_init_gdt));
	ctx->bsp_regs.vcpu_regs.gdt.limit = sizeof(acrn_init_gdt) - 1;
	ctx->bsp_regs.vcpu_regs.gdt.base = GDT_LOAD_OFF(ctx);

	/* CR0_ET | CR0_NE | CR0_PE */
	ctx->bsp_regs.vcpu_regs.cr0 = 0x31U;

	ctx->bsp_regs.vcpu_regs.cs_ar = 0xCF9BU;
	ctx->bsp_regs.vcpu_regs.cs_sel = 0x8U;
	ctx->bsp_regs.vcpu_regs.cs_limit = 0xFFFFFFFFU;

	ctx->bsp_regs.vcpu_regs.ds_sel = 0x10U;
	ctx->bsp_regs.vcpu_regs.ss_sel = 0x10U;
	ctx->bsp_regs.vcpu_regs.es_sel = 0x10U;
	ctx->bsp_regs.vcpu_regs.gs_sel = 0x10U;
	ctx->bsp_regs.vcpu_regs.fs_sel = 0x10U;

	ctx->bsp_regs.vcpu_regs.rip = entry;
	ctx->bsp_regs.vcpu_regs.gprs.rax = MULTIBOOT_MACHINE_STATE_MAGIC;

	if (multiboot_image == 1) {
		mi = (struct multiboot_info *)(ctx->baseaddr + MULTIBOOT_OFFSET);
		memset(mi, 0, sizeof(*mi));

		if (multiboot_flags == (1 << 1)) {
			/* Now, we only support elf binary request multiboot
			 * info with memory info filled case.
			 *
			 * TODO:
			 * For other elf image with multiboot enabled, they
			 * may need more fileds initialized here. We will add
			 * them here per each requirement.
			 */
			mi->flags = MULTIBOOT_INFO_MEMORY;
			mi->mem_lower = 0;
			mi->mem_upper = GDT_LOAD_OFF(ctx) / 1024U;
			ctx->bsp_regs.vcpu_regs.gprs.rbx = MULTIBOOT_OFFSET;
		} else {
			fprintf(stderr,
				"Invalid multiboot header in elf binary\n");
			return -1;
		}
	}

	return 0;
}
