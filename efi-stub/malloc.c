/*
 * Copyright (c) 2011, Intel Corporation
 * All rights reserved.
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

/**
 * memory_map - Allocate and fill out an array of memory descriptors
 * @map_buf: buffer containing the memory map
 * @map_size: size of the buffer containing the memory map
 * @map_key: key for the current memory map
 * @desc_size: size of the desc
 * @desc_version: memory descriptor version
 *
 * On success, @map_size contains the size of the memory map pointed
 * to by @map_buf and @map_key, @desc_size and @desc_version are
 * updated.
 */
EFI_STATUS
memory_map(EFI_MEMORY_DESCRIPTOR **map_buf, UINTN *map_size,
	   UINTN *map_key, UINTN *desc_size, UINT32 *desc_version)
{
	EFI_STATUS err;

	*map_size = sizeof(**map_buf) * 31;
get_map:

	/*
	 * Because we're about to allocate memory, we may
	 * potentially create a new memory descriptor, thereby
	 * increasing the size of the memory map. So increase
	 * the buffer size by the size of one memory
	 * descriptor, just in case.
	 */
	*map_size += sizeof(**map_buf);

	err = allocate_pool(EfiLoaderData, *map_size,
			    (void **)map_buf);
	if (err != EFI_SUCCESS) {
		Print(L"Failed to allocate pool for memory map");
		goto failed;
	}

	err = get_memory_map(map_size, *map_buf, map_key,
			     desc_size, desc_version);
	if (err != EFI_SUCCESS) {
		if (err == EFI_BUFFER_TOO_SMALL) {
			/*
			 * 'map_size' has been updated to reflect the
			 * required size of a map buffer.
			 */
			free_pool((void *)*map_buf);
			goto get_map;
		}

		Print(L"Failed to get memory map");
		goto failed;
	}

failed:
	return err;
}


EFI_STATUS
emalloc_reserved_aligned(EFI_PHYSICAL_ADDRESS *addr, UINTN size, UINTN align,
		EFI_PHYSICAL_ADDRESS maxaddr)
{
	UINTN msize, mkey, desc_sz, desc_addr, pages;
	UINT32 desc_version;
	EFI_MEMORY_DESCRIPTOR *mbuf;
	EFI_STATUS err;

	pages = EFI_SIZE_TO_PAGES(size);

	err = memory_map(&mbuf, &msize, &mkey, &desc_sz, &desc_version);
	if (err != EFI_SUCCESS) {
		goto fail;
	}

	/* In most time, Memory map reported by BIOS is an ordering list from low to hight.
	 * Scan it from high to low, so that allocate memory as high as possible
	 */
	for (desc_addr = (UINTN)mbuf + msize - desc_sz; desc_addr >= (UINTN)mbuf; desc_addr -= desc_sz) {
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
	if (desc_addr < (UINTN)mbuf) {
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

