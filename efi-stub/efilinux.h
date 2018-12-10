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
 *
 * This file contains some wrappers around the gnu-efi functions. As
 * we're not going through uefi_call_wrapper() directly, this allows
 * us to get some type-safety for function call arguments and for the
 * compiler to check that the number of function call arguments is
 * correct.
 *
 * It's also a good place to document the EFI interface.
 */

#ifndef __EFILINUX_H__
#define __EFILINUX_H__

#define EFILINUX_VERSION_MAJOR 1
#define EFILINUX_VERSION_MINOR 0

#define MEM_ADDR_1MB (1U << 20U)
#define MEM_ADDR_4GB (0xFFFFFFFFU)


extern EFI_SYSTEM_TABLE *sys_table;
extern EFI_BOOT_SERVICES *boot;

/**
 * allocate_pages - Allocate memory pages from the system
 * @atype: type of allocation to perform
 * @mtype: type of memory to allocate
 * @num_pages: number of contiguous 4KB pages to allocate
 * @memory: used to return the address of allocated pages
 *
 * Allocate @num_pages physically contiguous pages from the system
 * memory and return a pointer to the base of the allocation in
 * @memory if the allocation succeeds. On success, the firmware memory
 * map is updated accordingly.
 *
 * If @atype is AllocateAddress then, on input, @memory specifies the
 * address at which to attempt to allocate the memory pages.
 */
static inline EFI_STATUS
allocate_pages(EFI_ALLOCATE_TYPE atype, EFI_MEMORY_TYPE mtype,
	       UINTN num_pages, EFI_PHYSICAL_ADDRESS *memory)
{
	return uefi_call_wrapper(boot->AllocatePages, 4, atype,
				 mtype, num_pages, memory);
}

/**
 * free_pages - Return memory allocated by allocate_pages() to the firmware
 * @memory: physical base address of the page range to be freed
 * @num_pages: number of contiguous 4KB pages to free
 *
 * On success, the firmware memory map is updated accordingly.
 */
static inline EFI_STATUS
free_pages(EFI_PHYSICAL_ADDRESS memory, UINTN num_pages)
{
	return uefi_call_wrapper(boot->FreePages, 2, memory, num_pages);
}

/**
 * allocate_pool - Allocate pool memory
 * @type: the type of pool to allocate
 * @size: number of bytes to allocate from pool of @type
 * @buffer: used to return the address of allocated memory
 *
 * Allocate memory from pool of @type. If the pool needs more memory
 * pages are allocated from EfiConventionalMemory in order to grow the
 * pool.
 *
 * All allocations are eight-byte aligned.
 */
static inline EFI_STATUS
allocate_pool(EFI_MEMORY_TYPE type, UINTN size, void **buffer)
{
	return uefi_call_wrapper(boot->AllocatePool, 3, type, size, buffer);
}

/**
 * free_pool - Return pool memory to the system
 * @buffer: the buffer to free
 *
 * Return @buffer to the system. The returned memory is marked as
 * EfiConventionalMemory.
 */
static inline EFI_STATUS free_pool(void *buffer)
{
	return uefi_call_wrapper(boot->FreePool, 1, buffer);
}

/**
 * get_memory_map - Return the current memory map
 * @size: the size in bytes of @map
 * @map: buffer to hold the current memory map
 * @key: used to return the key for the current memory map
 * @descr_size: used to return the size in bytes of EFI_MEMORY_DESCRIPTOR
 * @descr_version: used to return the version of EFI_MEMORY_DESCRIPTOR
 *
 * Get a copy of the current memory map. The memory map is an array of
 * EFI_MEMORY_DESCRIPTORs. An EFI_MEMORY_DESCRIPTOR describes a
 * contiguous block of memory.
 *
 * On success, @key is updated to contain an identifer for the current
 * memory map. The firmware's key is changed every time something in
 * the memory map changes. @size is updated to indicate the size of
 * the memory map pointed to by @map.
 *
 * @descr_size and @descr_version are used to ensure backwards
 * compatibility with future changes made to the EFI_MEMORY_DESCRIPTOR
 * structure. @descr_size MUST be used when the size of an
 * EFI_MEMORY_DESCRIPTOR is used in a calculation, e.g when iterating
 * over an array of EFI_MEMORY_DESCRIPTORs.
 *
 * On failure, and if the buffer pointed to by @map is too small to
 * hold the memory map, EFI_BUFFER_TOO_SMALL is returned and @size is
 * updated to reflect the size of a buffer required to hold the memory
 * map.
 */
static inline EFI_STATUS
get_memory_map(UINTN *size, EFI_MEMORY_DESCRIPTOR *map, UINTN *key,
	       UINTN *descr_size, UINT32 *descr_version)
{
	return uefi_call_wrapper(boot->GetMemoryMap, 5, size, map,
				 key, descr_size, descr_version);
}

/**
 * exit_boot_serivces - Terminate all boot services
 * @image: firmware-allocated handle that identifies the image
 * @key: key to the latest memory map
 *
 * This function is called when efilinux wants to take complete
 * control of the system. efilinux should not make calls to boot time
 * services after this function is called.
 */
static inline EFI_STATUS
exit_boot_services(EFI_HANDLE image, UINTN key)
{
	return uefi_call_wrapper(boot->ExitBootServices, 2, image, key);
}


/**
 * handle_protocol - Query @handle to see if it supports @protocol
 * @handle: the handle being queried
 * @protocol: the GUID of the protocol
 * @interface: used to return the protocol interface
 *
 * Query @handle to see if @protocol is supported. If it is supported,
 * @interface contains the protocol interface.
 */
static inline EFI_STATUS
handle_protocol(EFI_HANDLE handle, EFI_GUID *protocol, void **interface)
{
        return uefi_call_wrapper(boot->HandleProtocol, 3,
                                 handle, protocol, interface);
}


/*
 * emalloc_reserved_mem - it is called to allocate memory hypervisor itself
 * and trampoline code, and mark the allocate memory as EfiReserved memory
 * type so that SOS won't touch it during boot.
 * @addr: a pointer to the allocated address on success
 * @size: size in bytes of the requested allocation
 * @max_addr: the allocated memory must be no more than this threshold
 */
static inline EFI_STATUS emalloc_reserved_mem(EFI_PHYSICAL_ADDRESS *addr,
	UINTN size, EFI_PHYSICAL_ADDRESS max_addr)
{
	*addr = max_addr;
	return allocate_pages(AllocateMaxAddress, EfiReservedMemoryType,
		EFI_SIZE_TO_PAGES(size), addr);
}


/*
 * emalloc_fixed_addr - it is called to allocate memory hypervisor itself
 * when CONFIG_RELOC config is NOT enable.And mark the allocated memory as
 * EfiReserved memory type so that SOS won't touch it during boot.
 * @addr: a pointer to the allocated address on success
 * @size: size in bytes of the requested allocation
 */
static inline EFI_STATUS emalloc_fixed_addr(EFI_PHYSICAL_ADDRESS *addr,
	UINTN size, EFI_PHYSICAL_ADDRESS fixed_addr)
{
	*addr = fixed_addr;
	return allocate_pages(AllocateAddress, EfiReservedMemoryType,
		EFI_SIZE_TO_PAGES(size), addr);
}

/**
 * exit - Terminate a loaded EFI image
 * @image: firmware-allocated handle that identifies the image
 * @status: the image's exit code
 * @size: size in bytes of @reason. Ignored if @status is EFI_SUCCESS
 * @reason: a NUL-terminated status string, optionally followed by binary data
 *
 * This function terminates @image and returns control to the boot
 * services. This function MUST NOT be called until all loaded child
 * images have exited. All memory allocated by the image must be freed
 * before calling this function, apart from the buffer @reason, which
 * will be freed by the firmware.
 */
static inline EFI_STATUS
exit(EFI_HANDLE image, EFI_STATUS status, UINTN size, CHAR16 *reason)
{
	return uefi_call_wrapper(boot->Exit, 4, image, status, size, reason);
}

#define PAGE_SIZE	4096

static const CHAR16 *memory_types[] = {
	L"EfiReservedMemoryType",
	L"EfiLoaderCode",
	L"EfiLoaderData",
	L"EfiBootServicesCode",
	L"EfiBootServicesData",
	L"EfiRuntimeServicesCode",
	L"EfiRuntimeServicesData",
	L"EfiConventionalMemory",
	L"EfiUnusableMemory",
	L"EfiACPIReclaimMemory",
	L"EfiACPIMemoryNVS",
	L"EfiMemoryMappedIO",
	L"EfiMemoryMappedIOPortSpace",
	L"EfiPalCode",
};

static inline const CHAR16 *memory_type_to_str(UINT32 type)
{
	if (type > sizeof(memory_types)/sizeof(CHAR16 *))
		return L"Unknown";

	return memory_types[type];
}

extern EFI_STATUS memory_map(EFI_MEMORY_DESCRIPTOR **map_buf,
			     UINTN *map_size, UINTN *map_key,
			     UINTN *desc_size, UINT32 *desc_version);
			     
#endif /* __EFILINUX_H__ */
