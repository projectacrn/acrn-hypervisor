/*
 * Copyright (c) 2011 - 2021, Intel Corporation
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
#include "boot.h"
#include "container.h"

EFI_SYSTEM_TABLE *sys_table;
EFI_BOOT_SERVICES *boot;
EFI_RUNTIME_SERVICES *runtime;
HV_LOADER hvld;

EFI_STATUS
get_efi_memmap(struct efi_memmap_info *mi, int size_only)
{
	UINTN map_size, map_key;
	UINT32 desc_version;
	UINTN desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	EFI_STATUS err = EFI_SUCCESS;

	/* We're just interested in the map's size for now */
	map_size = 0;
	err = get_memory_map(&map_size, NULL, NULL, &desc_size, NULL);
	if (err != EFI_SUCCESS && err != EFI_BUFFER_TOO_SMALL)
		goto out;

	if (size_only) {
		mi->map_size = map_size;
		mi->desc_size = desc_size;
		return err;
	}

again:
	err = allocate_pool(EfiLoaderData, map_size, (void **) &map_buf);
	if (err != EFI_SUCCESS)
		goto out;

	/*
	 * Remember! We've already allocated map_buf with emalloc (and
	 * 'map_size' contains its size) which means that it should be
	 * positioned below our allocation for the kernel. Use that
	 * space for the memory map.
	 */
	err = get_memory_map(&map_size, map_buf, &map_key,
				 &desc_size, &desc_version);
	if (err != EFI_SUCCESS) {
		if (err == EFI_BUFFER_TOO_SMALL) {
			/*
			 * Argh! The buffer that we allocated further
			 * up wasn't large enough which means we need
			 * to allocate them again, but this time
			 * larger. 'map_size' has been updated by the
			 * call to memory_map().
			 */
			free_pool(map_buf);
			goto again;
		}
		goto out;
	}

	mi->map_size = map_size;
	mi->map_key = map_key;
	mi->desc_version = desc_version;
	mi->desc_size = desc_size;
	mi->mmap = map_buf;

out:
	return err;
}

static EFI_STATUS
terminate_boot_services(EFI_HANDLE image, struct efi_memmap_info *mmap_info)
{
	EFI_STATUS err = EFI_SUCCESS;

	err = exit_boot_services(image, mmap_info->map_key);
	if (err != EFI_SUCCESS) {
		if (err == EFI_INVALID_PARAMETER) {
			/*
			 * Incorrect map key: memory map changed during the call of get_memory_map
			 * and exit_boot_services.
			 * We must call get_memory_map and exit_boot_services one more time.
			 * We can't allocate nor free pool since exit_boot_services has already been called.
			 * Original memory pool should be sufficient and this call is expected to succeed.
			 */
			err = get_memory_map(&mmap_info->map_size, mmap_info->mmap,
				&mmap_info->map_key, &mmap_info->desc_size, &mmap_info->desc_version);
			if (err != EFI_SUCCESS)
				goto out;

			err = exit_boot_services(image, mmap_info->map_key);
			if (err != EFI_SUCCESS)
				goto out;
		}
	}

out:
	return err;
}

static inline void hv_jump(EFI_PHYSICAL_ADDRESS hv_entry, uint32_t mbi, int32_t magic)
{
	asm volatile (
		"cli\n\t"
		"jmp *%2\n\t"
		:
		: "a"(magic), "b"(mbi), "r"(hv_entry)
		);
}

EFI_STATUS construct_mbi(HV_LOADER hvld, struct multiboot_info **mbinfo, struct efi_memmap_info *mmap_info)
{
	EFI_STATUS err = EFI_SUCCESS;
	return err;
}

EFI_STATUS
construct_mbi2(struct hv_loader *hvld, void **mbi_addr, struct efi_memmap_info *mmap_info)
{
	return EFI_SUCCESS;
}

static EFI_STATUS
run_acrn(EFI_HANDLE image, HV_LOADER hvld)
{
	EFI_STATUS err;
	struct efi_memmap_info memmapinfo;
	void *mbi;
	int32_t mb_version = hvld->get_multiboot_version(hvld);

	if (mb_version == 2) {
		err = construct_mbi2(hvld, &mbi, &memmapinfo);
	}
	else {
		err = construct_mbi(hvld, (struct multiboot_info **)&mbi, &memmapinfo);
	}

	if (err != EFI_SUCCESS)
		goto out;

	err = terminate_boot_services(image, &memmapinfo);
	if (err != EFI_SUCCESS)
		goto out;

	hv_jump(hvld->get_hv_entry(hvld), (uint32_t)(uint64_t)mbi,
		mb_version == 2 ? MULTIBOOT2_INFO_MAGIC : MULTIBOOT_INFO_MAGIC);

	/* Not reached on success */
out:
	return err;
}

/**
 * efi_main - The entry point for the OS loader image.
 * @image: firmware-allocated handle that identifies the image
 * @sys_table: EFI system table
 */
EFI_STATUS
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *_table)
{
	WCHAR *error_buf;
	EFI_STATUS err;
	EFI_LOADED_IMAGE *info;
	EFI_STATUS (*hvld_init)(EFI_LOADED_IMAGE *, HV_LOADER *);

	INTN index;

	InitializeLib(image, _table);
	sys_table = _table;
	boot = sys_table->BootServices;
	runtime = sys_table->RuntimeServices;

	if (CheckCrc(sys_table->Hdr.HeaderSize, &sys_table->Hdr) != TRUE)
		return EFI_LOAD_ERROR;

	err = handle_protocol(image, &LoadedImageProtocol, (void **)&info);
	if (err != EFI_SUCCESS)
		goto failed;

	/* We may support other containers in the future */
	hvld_init = container_init;

	/*
	 * Load hypervisor boot image handler. Currently Slim Bootloader
	 * compatible embedded container format is supported. File system
	 * mode to come future.
	 */
	err = hvld_init(info, &hvld);
	if (err != EFI_SUCCESS) {
		Print(L"Unable to init container library %r ", err);
		goto failed;
	}

	err = hvld->load_boot_image(hvld);
	if (err != EFI_SUCCESS) {
		Print(L"Unable to load ACRNHV Image %r ", err);
		goto failed;
	}

	err = hvld->load_modules(hvld);
	if (err != EFI_SUCCESS) {
		Print(L"Unable to load VM modules %r ", err);
		goto failed;
	}

	err = run_acrn(image, hvld);
	if (err != EFI_SUCCESS)
		goto failed;

	return EFI_SUCCESS;

failed:
	if (hvld) {
		hvld->deinit(hvld);
	}

	/*
	 * We need to be careful not to trash 'err' here. If we fail
	 * to allocate enough memory to hold the error string fallback
	 * to returning 'err'.
	 */
	if (allocate_pool(EfiLoaderData, ERROR_STRING_LENGTH,
			  (void **)&error_buf) != EFI_SUCCESS) {
		Print(L"Couldn't allocate pages for error string\n");
		return err;
	}

	StatusToString(error_buf, err);
	Print(L": %s\n", error_buf);

	/* If we don't wait for user input, (s)he will not see the error message */
	uefi_call_wrapper(sys_table->ConOut->OutputString, 2, sys_table->ConOut, \
					L"\r\n\r\n\r\nHit any key to exit\r\n");
	uefi_call_wrapper(sys_table->BootServices->WaitForEvent, 3, 1, \
					&sys_table->ConIn->WaitForKey, &index);

	return exit(image, err, ERROR_STRING_LENGTH, error_buf);
}
