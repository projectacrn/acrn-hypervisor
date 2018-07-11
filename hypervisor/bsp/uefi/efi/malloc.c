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

/**
 * emalloc - Allocate memory with a strict alignment requirement
 * @size: size in bytes of the requested allocation
 * @align: the required alignment of the allocation
 * @addr: a pointer to the allocated address on success
 *
 * If we cannot satisfy @align we return 0.
 *
 * FIXME: This function cannot guarantee to return address under 4G,
 * and the hypervisor cannot handle params, which address is above 4G,
 * delivered from efi stub.
 */
EFI_STATUS emalloc(UINTN size, UINTN align, EFI_PHYSICAL_ADDRESS *addr)
{
	UINTN map_size, map_key, desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	UINTN d, map_end;
	UINT32 desc_version;
	EFI_STATUS err;
	UINTN nr_pages = EFI_SIZE_TO_PAGES(size);

	err = memory_map(&map_buf, &map_size, &map_key,
			 &desc_size, &desc_version);
	if (err != EFI_SUCCESS)
		goto fail;

	d = (UINTN)map_buf;
	map_end = (UINTN)map_buf + map_size;

	for (; d < map_end; d += desc_size) {
		EFI_MEMORY_DESCRIPTOR *desc;
		EFI_PHYSICAL_ADDRESS start, end, aligned;

		desc = (EFI_MEMORY_DESCRIPTOR *)d;
		if (desc->Type != EfiConventionalMemory)
			continue;

		if (desc->NumberOfPages < nr_pages)
			continue;

		start = desc->PhysicalStart;
		end = start + (desc->NumberOfPages << EFI_PAGE_SHIFT);

		/* Low-memory is super-precious! */
		if (end <= 1 << 20)
			continue;
		if (start < 1 << 20) {
			size -= (1 << 20) - start;
			start = (1 << 20);
		}

		aligned = (start + align -1) & ~(align -1);

		if ((aligned + size) <= end) {
			err = allocate_pages(AllocateAddress, EfiLoaderData,
					     nr_pages, &aligned);
			if (err == EFI_SUCCESS) {
				*addr = aligned;
				break;
			}
		}
	}

	if (d == map_end)
		err = EFI_OUT_OF_RESOURCES;

	free_pool(map_buf);
fail:
	return err;
}

EFI_STATUS emalloc_for_low_mem(EFI_PHYSICAL_ADDRESS *addr, UINTN size)
{
	UINTN map_size, map_key, desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	UINTN d, map_end;
	UINT32 desc_version;
	EFI_STATUS err;
	UINTN nr_pages = EFI_SIZE_TO_PAGES(size);

	err = memory_map(&map_buf, &map_size, &map_key,
			 &desc_size, &desc_version);

	if (err != EFI_SUCCESS)
		goto fail;

	d = (UINTN)map_buf;
	map_end = (UINTN)map_buf + map_size;

	for (; d < map_end; d += desc_size) {
		EFI_MEMORY_DESCRIPTOR *desc;
		EFI_PHYSICAL_ADDRESS start, end, aligned;

		desc = (EFI_MEMORY_DESCRIPTOR *)d;
		if (desc->Type != EfiConventionalMemory)
			continue;

		if (desc->NumberOfPages < nr_pages)
			continue;

		start = desc->PhysicalStart;
		end = start + (desc->NumberOfPages << EFI_PAGE_SHIFT);
		size = nr_pages << EFI_PAGE_SHIFT;

		/* allocate in low memory only */
		if (start >= 1 << 20)
			continue;

		if (end > 1 << 20)
			end = (1 << 20);

		if (end - start >= size) {
			aligned = end - size;
			err = allocate_pages(AllocateAddress, EfiReservedMemoryType,
						 nr_pages, &aligned);
			if (err == EFI_SUCCESS) {
				*addr = aligned;
				break;
			}
		}
	}

	if (d == map_end)
		err = EFI_OUT_OF_RESOURCES;

	free_pool(map_buf);
fail:
	return err;
}

EFI_STATUS __emalloc(UINTN size, UINTN min_addr, EFI_PHYSICAL_ADDRESS *addr,
	EFI_MEMORY_TYPE mem_type)
{
	UINTN map_size, map_key, desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	UINTN d, map_end;
	UINT32 desc_version;
	EFI_STATUS err;
	UINTN nr_pages = EFI_SIZE_TO_PAGES(size);

	err = memory_map(&map_buf, &map_size, &map_key,
			 &desc_size, &desc_version);
	if (err != EFI_SUCCESS)
		goto fail;

	d = (UINTN)map_buf;
	map_end = (UINTN)map_buf + map_size;
	size = nr_pages << EFI_PAGE_SHIFT;

	for (; d < map_end; d += desc_size) {
		EFI_MEMORY_DESCRIPTOR *desc;
		EFI_PHYSICAL_ADDRESS start, end, aligned;

		desc = (EFI_MEMORY_DESCRIPTOR *)d;
		start = desc->PhysicalStart;
		end = start + (desc->NumberOfPages << EFI_PAGE_SHIFT);

		if ((min_addr > start) && (min_addr < end)) {
			start = min_addr;
		}

		start = (start + EFI_PAGE_SIZE - 1) & ~(EFI_PAGE_SIZE - 1);

		/*
		 * Low-memory is super-precious!
		 * Also we don't allocate from address over 4G
		 */
		if ((desc->Type != EfiConventionalMemory) ||
			(desc->NumberOfPages < nr_pages) ||
			(start < (1ULL << 20)) ||
			((start + size) > end) ||
			((start + size) >= (1UL << 32))) {
			continue;
		}

#ifndef CONFIG_RELOC
		aligned = start;
#else
		aligned = min_addr;
#endif
		err = allocate_pages(AllocateAddress, mem_type,
					 nr_pages, &aligned);
		if (err == EFI_SUCCESS) {
			*addr = aligned;
			break;
		}
	}

	if (d == map_end) {
		err = EFI_OUT_OF_RESOURCES;
	}

	free_pool(map_buf);
fail:
	return err;
}

/**
 * efree - Return memory allocated with emalloc
 * @memory: the address of the emalloc() allocation
 * @size: the size of the allocation
 */
void efree(EFI_PHYSICAL_ADDRESS memory, UINTN size)
{
	UINTN nr_pages = EFI_SIZE_TO_PAGES(size);

	free_pages(memory, nr_pages);
}

/**
 * malloc - Allocate memory from the EfiLoaderData pool
 * @size: size in bytes of the requested allocation
 *
 * Return a pointer to an allocation of @size bytes of type
 * EfiLoaderData.
 */
void *malloc(UINTN size)
{
	EFI_STATUS err;
	void *buffer;

	err = allocate_pool(EfiLoaderData, size, &buffer);
	if (err != EFI_SUCCESS)
		buffer = NULL;

	return buffer;
}

/**
 * free - Release memory to the EfiLoaderData pool
 * @buffer: pointer to the malloc() allocation to free
 */
void free(void *buffer)
{
	if (buffer)
		free_pool(buffer);
}

/**
 * calloc - Allocate zeroed memory for an array of elements
 * @nmemb: number of elements
 * @size: size of each element
 */
void *calloc(UINTN nmemb, UINTN size)
{
	void *buffer;

	/*
	 * There's no equivalent of UINTN_MAX, so for safety we refuse to
	 * allocate anything larger than 32 bits.
	 */
	UINTN bytes = nmemb * size;
	if ((nmemb | size) > 0xffffU) {
		if (size && bytes / size != nmemb)
			return NULL;
	}

	buffer = malloc(bytes);
	if (buffer)
		(void)memset(buffer, 0, bytes);
	return buffer;
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

