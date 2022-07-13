/*
 * Copyright (c) 2021 - 2022, Intel Corporation.
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
#include <stdint.h>
#include "stdlib.h"
#include "boot.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))


/**
 * @brief Load elf pointed by ehdr, linked at link_addr, to load_addr.
 *
 * @param[in] ehdr          A pointer to target ELF header.
 * @param[in] load_addr     The address to which the image will be loaded.
 * @param[in] link_addr     The address to which the image was linked.
 *
 * @return  0 on success, -1 on error.
 */
int elf_load(Elf32_Ehdr *ehdr, uint64_t load_addr, uint64_t link_addr)
{
	int i;
	uint64_t addr;
	Elf32_Phdr *phdr;
	Elf32_Phdr *pbase = (Elf32_Phdr *)((char *)ehdr + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = (Elf32_Phdr *)((char *)pbase + i * ehdr->e_phentsize);
		if ((phdr->p_type != PT_LOAD) || (phdr->p_memsz == 0) || (phdr->p_offset == 0)) {
			continue;
		}

		if (phdr->p_filesz > phdr->p_memsz) {
			return -1;
		}

		addr = (uint64_t)(load_addr + (phdr->p_paddr - link_addr));
		memcpy((char *)addr, (const char *)((char *)ehdr + phdr->p_offset), phdr->p_filesz);

		if (phdr->p_memsz > phdr->p_filesz) {
			addr = (uint64_t)(load_addr + (phdr->p_paddr - link_addr + phdr->p_filesz));
			(void)memset((void *)addr, 0x0, (phdr->p_memsz - phdr->p_filesz));
		}
	}

	return 0;
}

/**
 * @brief Calculate link address range of the elf image.
 *
 * @param[in] ehdr          A pointer to target ELF header.
 * @param[out] link_addr_low      The lowest link address of all loadable section in ELF.
 * @param[out] link_addr_high     The highest reachable link address (link address plus memory size of that section) of all
 *                                loadable section in ELF.
 *
 * @return  0 on success, -1 on error.
 */
int elf_calc_link_addr_range(Elf32_Ehdr *ehdr, uint64_t *link_addr_low, uint64_t *link_addr_high)
{
	Elf32_Phdr *phdr;
	Elf32_Phdr *pbase = (Elf32_Phdr *)((char *)ehdr + ehdr->e_phoff);

	int i;
	uint64_t ram_low = ~0;
	uint64_t ram_high = 0;

	for (i = 0; i < ehdr->e_phnum; i++) {
		phdr = (Elf32_Phdr *)((char *)pbase + i * ehdr->e_phentsize);
		if (phdr->p_type != PT_LOAD)
			continue;

		ram_low = MIN(ram_low, phdr->p_paddr);
		ram_high = MAX(ram_high, ALIGN_UP(phdr->p_paddr + phdr->p_memsz, phdr->p_align));
	}

	*link_addr_low = ram_low;
	*link_addr_high = ram_high;

	return 0;
}

/**
 * @brief Get entry point of ELF image.
 *
 * @param[in] ehdr A pointer to target ELF header.
 *
 * @return  Entry point address of ELF image.
 */
uint32_t elf_get_entry(Elf32_Ehdr *ehdr)
{
	return ehdr->e_entry;
}

/**
 * @brief Validate ELF header.
 *
 * @param[in] ehdr A pointer to target ELF header.
 *
 * @return  0 on success (valid elf), -1 on error.
 */
int validate_elf_header(Elf32_Ehdr *ehdr)
{
	if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
		ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
		Print(L"Image is not a valid arch-independent ELF\n");
		return -1;
	}

	if (ehdr->e_ident[EI_CLASS] != ELFCLASS32 ||
		ehdr->e_machine != EM_386 ||
		ehdr->e_version != EV_CURRENT) {
		Print(L"Image is not a valid arch-dependent ELF\n");
		return -1;
	}

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
		Print(L"This ELF is not of the right type\n");
		return -1;
	}

	return 0;
}
