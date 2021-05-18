/*
 * Copyright (c) 2017 - 2021, Intel Corporation. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <elf.h>
#include <efi.h>
#include <efilib.h>
#include "boot.h"
#include "stdlib.h"
#include "efilinux.h"
#include "multiboot.h"
#include "container.h"

EFI_STATUS load_acrn_elf(const UINT8 *elf_image, EFI_PHYSICAL_ADDRESS *hv_hpa)
{
	EFI_STATUS err = EFI_SUCCESS;
	int i;

	EFI_PHYSICAL_ADDRESS addr = 0u;

	Elf32_Ehdr *ehdr  = (Elf32_Ehdr *)(elf_image);
	Elf32_Phdr *pbase = (Elf32_Phdr *)(elf_image + ehdr->e_phoff);
	Elf32_Phdr *phdr  = (Elf32_Phdr *)(elf_image + ehdr->e_phoff);

	/* without relocateion enabled, hypervisor binary need to reside in
	 * fixed memory address starting from CONFIG_HV_RAM_START, make a call
	 * to emalloc_fixed_addr for that case. With CONFIG_RELOC enabled,
	 * hypervisor is able to do relocation, the only requirement is that
	 * it need to reside in memory below 4GB, call emalloc_reserved_mem()
	 * instead.
	 *
	 * Don't relocate hypervisor binary under 256MB, which could be where
	 * guest Linux kernel boots from, and other usage, e.g. hvlog buffer
	*/
#ifdef CONFIG_RELOC
	err = emalloc_reserved_aligned(hv_hpa, CONFIG_HV_RAM_SIZE, 2U * MEM_ADDR_1MB,
			256U * MEM_ADDR_1MB, MEM_ADDR_4GB);
#else
	err = emalloc_fixed_addr(hv_hpa, CONFIG_HV_RAM_SIZE, CONFIG_HV_RAM_START);
#endif

	if (err != EFI_SUCCESS) {
		Print(L"Failed to allocate memory for ACRN HV %r\n", err);
		goto out;
	}

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = (Elf32_Phdr *)((UINT8 *)pbase + i * ehdr->e_phentsize);
		if ((phdr->p_type != PT_LOAD) || (phdr->p_memsz == 0) || (phdr->p_offset == 0)) {
			continue;
		}

		if (phdr->p_filesz > phdr->p_memsz) {
			err = EFI_LOAD_ERROR;
			goto out;
		}

		addr = (EFI_PHYSICAL_ADDRESS)(*hv_hpa + (phdr->p_paddr - CONFIG_HV_RAM_START));
		memcpy((char *)addr, (const char *)(elf_image + phdr->p_offset), phdr->p_filesz);

		if (phdr->p_memsz > phdr->p_filesz) {
			addr = (EFI_PHYSICAL_ADDRESS)(*hv_hpa + (phdr->p_paddr - CONFIG_HV_RAM_START + phdr->p_filesz));
			(void)memset((void *)addr, 0x0, (phdr->p_memsz - phdr->p_filesz));
		}
	}

out:
	return err;
}

EFI_STATUS load_images_from_container(EFI_LOADED_IMAGE *info, struct hv_boot_info *hv_info)
{
	EFI_STATUS err = EFI_SUCCESS;
	char *section  = NULL;
	UINTN sec_addr = 0u;
	UINTN sec_size = 0u;

	int i;
	CONTAINER_HDR   *hdr  = NULL;
	COMPONENT_ENTRY *comp = NULL;

	EFI_PHYSICAL_ADDRESS addr = 0u;
	struct multiboot_module *mods = NULL;

	section = ".hv";
	err = get_pe_section(info->ImageBase, section, strlen(section), &sec_addr, &sec_size);
	if (EFI_ERROR(err)) {
		Print(L"Unable to locate section of ACRNHV Container %r ", err);
		goto out;
	}

	hdr = (CONTAINER_HDR*)(info->ImageBase + sec_addr);
	hv_info->mods_count = (hdr->Count - 3) / 2;
	mods = hv_info->mods;

	comp = (COMPONENT_ENTRY *)(hdr + 1);
	for (i = 0; i < hdr->Count; i++) {
		const UINTN offset = hdr->DataOffset + comp->Offset;
		LOADER_COMPRESSED_HEADER *lzh = (LOADER_COMPRESSED_HEADER *)((UINT8 *)(hdr) + offset);

		if (i == 0) {
			/* hv_cmdline */
			if (lzh->Size) {
				err = allocate_pool(EfiLoaderData, lzh->Size + hv_info->cmdline_sz, (VOID *)&addr);
				if (err != EFI_SUCCESS) {
					Print(L"Failed to allocate memory for hv_string %r\n", i, err);
					goto out;
				}
				(void)memset((void *)addr, 0x0, lzh->Size + hv_info->cmdline_sz);
				memcpy((char *)addr, (const char *)lzh->Data, lzh->Size - 1);
				if (hv_info->cmdline_sz) {
					*((char *)(addr) + lzh->Size - 1) = ' ';
					memcpy((char *)(addr) + lzh->Size, (const char *)hv_info->cmdline, hv_info->cmdline_sz);
					hv_info->cmdline_sz += lzh->Size;
					free_pool((void *)hv_info->cmdline);
				}
				hv_info->cmdline = (char *)addr;
			}
		} else if (i == 1) {
			/* acrn.32.out elf */
			err = load_acrn_elf((const UINT8 *)lzh->Data, &hv_info->hv_hpa);
			if (err != EFI_SUCCESS) {
				Print(L"Failed to load ACRN HV ELF Image%r\n", err);
				goto out;
			}
		} else if (i == (hdr->Count - 1)) {
			/* sbl signature, should be ignored since entire image shall be verified by shim */
		} else {
			addr = 0;
			if ((i % 2) == 0) {
				/* vm0_tag.txt, vm1_tag.txt, acpi_vm0.txt */
				err = allocate_pool(EfiLoaderData, lzh->Size, (VOID *)&addr);
				if (err != EFI_SUCCESS) {
					Print(L"Failed to allocate memory for module[%d] string %r\n", i, err);
					goto out;
				}
				memcpy((char *)addr, (const char *)lzh->Data, lzh->Size);
				mods->mmo_string = addr;
			} else {
				/* vm0_kernel, vm1_kernel, acpi_vm0.bin */
				err = emalloc_reserved_aligned(&addr, lzh->Size, MEM_ADDR_1MB, 256U * MEM_ADDR_1MB, MEM_ADDR_4GB);
				if (err != EFI_SUCCESS) {
					Print(L"Failed to allocate memory for module[%d] image %r\n", i, err);
					goto out;
				}
				memcpy((char *)addr, (const char *)lzh->Data, lzh->Size);
				mods->mmo_start = addr;
				mods->mmo_end   = mods->mmo_start + lzh->Size;
				mods++;
			}
		}
		comp = (COMPONENT_ENTRY *)((UINT8 *)(comp + 1) + comp->HashSize);
	}

out:
	if (err != EFI_SUCCESS) {
		if (hv_info->hv_hpa) {
			free_pages(hv_info->hv_hpa, EFI_SIZE_TO_PAGES(CONFIG_HV_RAM_SIZE));
			hv_info->hv_hpa = 0;
		}
		for (i = 0; i < hv_info->mods_count; i++) {
			mods = &hv_info->mods[i];
			free_pool((void *)(EFI_PHYSICAL_ADDRESS)mods->mmo_string);
			free_pages(mods->mmo_start, EFI_SIZE_TO_PAGES(mods->mmo_end - mods->mmo_start));
		}
		(void)memset((void *)hv_info->mods, 0x0, sizeof(struct multiboot_module) * hv_info->mods_count);
		hv_info->mods_count = 0u;
	}
	return err;
}
