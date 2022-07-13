/*
 * Copyright (c) 2011 - 2022, Intel Corporation.
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

#include <efi.h>
#include <efilib.h>
#include "efilinux.h"
#include "stdlib.h"
#include "boot.h"

EFI_STATUS
emalloc_reserved_aligned(EFI_PHYSICAL_ADDRESS *addr, UINTN size, UINTN align,
		EFI_PHYSICAL_ADDRESS minaddr, EFI_PHYSICAL_ADDRESS maxaddr)
{
	struct efi_memmap_info mmap_info;
	UINTN msize, desc_sz, desc_addr, pages;
	EFI_MEMORY_DESCRIPTOR *mbuf;
	EFI_STATUS err;

	pages = EFI_SIZE_TO_PAGES(size);

	/* Memory map may change so we request it again */
	err = get_efi_memmap(&mmap_info, 0);
	if (err != EFI_SUCCESS)
		goto fail;

	msize = mmap_info.map_size;
	desc_sz = mmap_info.desc_size;
	mbuf = mmap_info.mmap;

	/* ACRN requests for lowest possible address that's greater than minaddr */
	/* TODO: in the future we may want to support both preferences */
	for (desc_addr = (UINTN)mbuf; desc_addr <= (UINTN)mbuf + msize - desc_sz; desc_addr += desc_sz) {
		EFI_MEMORY_DESCRIPTOR *desc;
		EFI_PHYSICAL_ADDRESS start, end;

		desc = (EFI_MEMORY_DESCRIPTOR*)desc_addr;
		if (desc->Type != EfiConventionalMemory)
			continue;

		start = desc->PhysicalStart;
		end = start + (desc->NumberOfPages << EFI_PAGE_SHIFT);

		/* 1MB low memory is allocated only if required/requested */
		if ((end <= MEM_ADDR_1MB) && (maxaddr > MEM_ADDR_1MB))
			continue;

		/* starting allocation from 1M above unless requested */
		if ((start < MEM_ADDR_1MB) && (maxaddr > MEM_ADDR_1MB)) {
			start = MEM_ADDR_1MB;
		}

		/* zero page won't be allocated */
		if (start < 4096) {
			start = 4096;
		}

		if (start < minaddr) {
			start = minaddr;
		}
		start = (start + align - 1) & ~(align - 1);

		 /* Since this routine is called during booting, memory block is large
		  * enought, the reduction of memory size for memory alignment won't
		  * impact allocation. It is true in most cases. if it is not true, loop
		  * again
		  */
		if ((start + size <= end) && (start + size <= maxaddr)) {
			err = allocate_pages(AllocateAddress, EfiReservedMemoryType, pages, &start);
			if (err == EFI_SUCCESS) {
				*addr = start;
				break;
			}
		}
	}
	if (desc_addr > (UINTN)mbuf + msize - desc_sz) {
		err = EFI_OUT_OF_RESOURCES;
	}

	free_pool(mbuf);

fail:
	return err;
}

EFI_STATUS dump_e820(void)
{
	UINTN map_size, map_key, desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	UINTN d, map_end;
	UINTN i;
	UINT32 desc_version;
	EFI_STATUS err;

	err = memory_map(&map_buf, &map_size, &map_key,
			 &desc_size, &desc_version);
	if (err != EFI_SUCCESS)
		goto fail;

	d = (UINTN)map_buf;
	map_end = (UINTN)map_buf + map_size;

	for (i = 0; d < map_end; d += desc_size, i++) {
		EFI_MEMORY_DESCRIPTOR *desc;
		EFI_PHYSICAL_ADDRESS start, end;

		desc = (EFI_MEMORY_DESCRIPTOR *)d;
		if (desc->Type != EfiConventionalMemory)
			continue;

		start = desc->PhysicalStart;
		end = start + (desc->NumberOfPages << EFI_PAGE_SHIFT);

	    Print(L"[%d]start:%lx, end:%lx, type:%d\n", i, start, end, desc->Type);
	}

	free_pool(map_buf);
fail:
	return err;
}

