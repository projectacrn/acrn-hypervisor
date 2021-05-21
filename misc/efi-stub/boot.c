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
#include "boot.h"
#include "acrn_common.h"
#include "MpService.h"
#include "container.h"

EFI_SYSTEM_TABLE *sys_table;
EFI_BOOT_SERVICES *boot;
static struct efi_memmap_info mmap_info;
static struct acpi_table_rsdp *rsdp = NULL;
HV_LOADER hvld;

static EFI_STATUS
get_current_memory_map(struct efi_memmap_info *mi)
{
	UINTN map_size, map_key;
	UINT32 desc_version;
	UINTN desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	EFI_STATUS err = EFI_SUCCESS;

	/* We're just interested in the map's size for now */
	map_size = 0;
	err = get_memory_map(&map_size, NULL, NULL, NULL, NULL);
	if (err != EFI_SUCCESS && err != EFI_BUFFER_TOO_SMALL)
		goto out;

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

static void
enable_disable_all_ap(BOOLEAN enable)
{
	EFI_MP_SERVICES_PROTOCOL *mp = NULL;
	EFI_STATUS err;
	EFI_GUID mp_guid = EFI_MP_SERVICES_PROTOCOL_GUID;
	UINTN n_proc, n_enabled_proc, bsp, i;

	err = uefi_call_wrapper(boot->LocateProtocol, 3, &mp_guid, NULL, (void **)&mp);
	if (err != EFI_SUCCESS) {
		Print(L"Unable to locate MP service protocol: %r, skip %s all AP\n",
				err, enable ? "enable" : "disable");
		return;
	}

	err = uefi_call_wrapper(mp->GetNumberOfProcessors, 3, mp, &n_proc, &n_enabled_proc);
	if (err != EFI_SUCCESS) {
		Print(L"failed to GetNumberOfProcessors: %r\n", err);
		return;
	}
	Print(L"detected %d processes, %d enabled\n", n_proc, n_enabled_proc);

	err = uefi_call_wrapper(mp->WhoAmI, 2, mp, &bsp);
	if (err != EFI_SUCCESS) {
		Print(L"failed to WhoAmI: %r\n", err);
		return;
	}
	Print(L"current on process %d\n", bsp);

	for (i = 0; i < n_proc; i++) {
		if (i == bsp) {
			continue;
		}

		err = uefi_call_wrapper(mp->EnableDisableAP, 4, mp, i, enable, NULL);
		if (err != EFI_SUCCESS) {
			Print(L"failed to %s AP%d: %r\n", enable ? "enable" : "disable", i, err);
		}
	}
}

static EFI_STATUS
terminate_boot_services(EFI_HANDLE image, UINTN map_key)
{
	EFI_STATUS err = EFI_SUCCESS;

	err = exit_boot_services(image, map_key);
	if (err != EFI_SUCCESS) {
		if (err == EFI_INVALID_PARAMETER) {
			/*
			 * Incorrect map key: memory map changed during the call of get_memory_map
			 * and exit_boot_services.
			 * We must call get_memory_map and exit_boot_services one more time.
			 * We can't allocate nor free pool since exit_boot_services has already been called.
			 * Original memory pool should be sufficient and this call is expected to succeed.
			 */
			UINTN map_size, desc_size;
			UINT32 desc_version;

			err = get_memory_map(&map_size, mmap_info.mmap, &map_key, &desc_size, &desc_version);
			if (err != EFI_SUCCESS)
				goto out;

			err = exit_boot_services(image, map_key);
			if (err != EFI_SUCCESS)
				goto out;
		}
	}

out:
	return err;
}

static inline void hv_jump(EFI_PHYSICAL_ADDRESS hv_start,
		struct multiboot_info *mbi)
{
	hv_func hf;

	/* The 64-bit entry of acrn hypervisor is 0x1200 from the start
	 * address of hv image.
	 */
	hf = (hv_func)(hv_start + 0x1200);

	asm volatile ("cli");

	/* jump to acrn hypervisor */
	hf(MBI_INFO_MAGIC, mbi);
}

static EFI_STATUS
fill_e820(HV_LOADER hvld, struct multiboot_mmap *mmap, int32_t mmap_entry_count, int32_t *e820_count)
{
	EFI_STATUS err = EFI_SUCCESS;
	int32_t i, j;

	/*
	 * Convert the EFI memory map to E820.
	 */
	for (i = 0, j = 0; i < mmap_entry_count && j < MBOOT_MMAP_NUMS - 1; i++) {
		EFI_MEMORY_DESCRIPTOR *d;
		uint32_t e820_type = 0;

		d = (EFI_MEMORY_DESCRIPTOR *)((uint64_t)mmap_info.mmap + (i * mmap_info.desc_size));
		switch(d->Type) {
		case EfiReservedMemoryType:
		case EfiRuntimeServicesCode:
		case EfiRuntimeServicesData:
		case EfiMemoryMappedIO:
		case EfiMemoryMappedIOPortSpace:
		case EfiPalCode:
			e820_type = E820_RESERVED;
			break;

		case EfiUnusableMemory:
			e820_type = E820_UNUSABLE;
			break;

		case EfiACPIReclaimMemory:
			e820_type = E820_ACPI;
			break;

		case EfiLoaderCode:
		case EfiLoaderData:
		case EfiBootServicesCode:
		case EfiBootServicesData:
		case EfiConventionalMemory:
			e820_type = E820_RAM;
			break;

		case EfiACPIMemoryNVS:
			e820_type = E820_NVS;
			break;

		default:
			continue;
		}

		if ((j != 0) && mmap[j-1].mm_type == e820_type &&
			(mmap[j-1].mm_base_addr + mmap[j-1].mm_length)
			== d->PhysicalStart) {
			mmap[j-1].mm_length += d->NumberOfPages << EFI_PAGE_SHIFT;
		} else {
			mmap[j].mm_base_addr = d->PhysicalStart;
			mmap[j].mm_length = d->NumberOfPages << EFI_PAGE_SHIFT;
			mmap[j].mm_type = e820_type;
			j++;
		}
	}

	/*
	 * if we haven't gone through all the mmap table entries,
	 * there must be a memory overwrite if we continue,
	 * so just abort anyway.
	 */
	if (i < mmap_entry_count) {
		Print(L": bios provides %d mmap entries which is beyond limitation[%d]\n",
				mmap_entry_count, MBOOT_MMAP_NUMS-1);
		err = EFI_INVALID_PARAMETER;
		goto out;
	}

	/* switch hv memory region(0x20000000 ~ 0x22000000) to
	 * available RAM in e820 table
	 */
	mmap[j].mm_base_addr = hvld->get_hv_hpa(hvld);
	mmap[j].mm_length = CONFIG_HV_RAM_SIZE;
	mmap[j].mm_type = E820_RAM;
	j++;

	mmap[j].mm_base_addr = hvld->get_mod_hpa(hvld);
	mmap[j].mm_length = hvld->get_total_modsize(hvld);
	mmap[j].mm_type = E820_RAM;
	j++;

	*e820_count = j;
out:
	return err;
}


EFI_STATUS construct_mbi(HV_LOADER hvld, struct multiboot_info *mbi,
		struct multiboot_mmap *mmap, struct multiboot_module *mods)
{
	EFI_STATUS err = EFI_SUCCESS;
	int32_t mmap_entry_count, e820_count = 0;

	mmap_entry_count = mmap_info.map_size / mmap_info.desc_size;

	err = fill_e820(hvld, mmap, mmap_entry_count, &e820_count);
	if (err != EFI_SUCCESS)
		goto out;

//	mbi->mi_cmdline = (UINTN)hv_info->cmdline;
	mbi->mi_mmap_addr = (UINTN)mmap;
	mbi->mi_mmap_length = e820_count*sizeof(struct multiboot_mmap);
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP | MULTIBOOT_INFO_HAS_CMDLINE;
out:
	return err;
}

#ifdef CONFIG_MULTIBOOT2
static uint32_t
get_mbi2_size(struct hv_loader *hvld)
{
    return 2 * sizeof(uint32_t)
		+ sizeof(struct multiboot2_tag)
		+ sizeof(struct multiboot2_tag)
		+ (sizeof(struct multiboot2_tag_string) + ALIGN_UP(hvld->get_boot_cmdsize(hvld), MULTIBOOT2_TAG_ALIGN))
		+ (hvld->get_mod_count(hvld) * sizeof(struct multiboot2_tag_module) + hvld->get_total_modcmdsize(hvld))
		+ ALIGN_UP((sizeof(struct multiboot2_tag_mmap) + (mmap_info.map_size / mmap_info.desc_size) * sizeof(struct multiboot2_mmap_entry)), MULTIBOOT2_TAG_ALIGN)
		+ ALIGN_UP(sizeof(struct multiboot2_tag_new_acpi) + rsdp->length, MULTIBOOT2_TAG_ALIGN)
		+ ALIGN_UP(sizeof(struct multiboot2_tag_efi64), MULTIBOOT2_TAG_ALIGN)
		+ ALIGN_UP(sizeof(struct multiboot2_tag_efi_mmap) + ALIGN_UP(mmap_info.map_size + 0x1000, 0x1000), /* Add an extra page since UEFI can alter the memory map */
			MULTIBOOT2_TAG_ALIGN);
}

EFI_STATUS
construct_mbi2(struct hv_loader *hvld, void **mbi_addr, struct multiboot_mmap *mmap)
{
	uint64_t *mbistart;
	uint64_t *p;
	uint32_t mbi2_size;
	EFI_STATUS err;

	/* Get size only for mbi size calculation */
	err = get_memory_map((UINTN *)&(mmap_info.map_size), NULL, NULL, (UINTN *)&(mmap_info.desc_size), 0);
	if (err != EFI_SUCCESS && err != EFI_BUFFER_TOO_SMALL)
		return err;

	mbi2_size = get_mbi2_size(hvld);

	/* per UEFI spec v2.9: This allocation is guaranteed to be 8-bytes aligned */
	err = allocate_pool(EfiLoaderData, mbi2_size, (void **)&mbistart);
	if (err != EFI_SUCCESS)
		goto out;

	/*
	 * Get full memory map again.
	 * We have just allocated memory and the mmap_info will be different.
	 */
	err = get_current_memory_map(&mmap_info);
	if (err != EFI_SUCCESS)
		goto out;

	/* total_size and reserved */
	p = mbistart;
	p += (2 * sizeof(uint32_t)) / sizeof(uint64_t);

	/* Boot command line */
	{
		struct multiboot2_tag_string *tag = (struct multiboot2_tag_string *)p;
		(void)hvld->fill_bootcmd_tag(hvld, tag);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* Boot loader name */
	{
		struct multiboot2_tag_string *tag = (struct multiboot2_tag_string *)p;
		uint32_t boot_loader_name_size = strlen(UEFI_BOOT_LOADER_NAME);
		tag->type = MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME;
		tag->size = sizeof(struct multiboot2_tag_string) + boot_loader_name_size;
		memcpy(tag->string, UEFI_BOOT_LOADER_NAME, boot_loader_name_size);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* Modules */
	{
		unsigned i;
		uint32_t mod_count = hvld->get_mod_count(hvld);
		for (i = 0; i < mod_count; i++) {
			struct multiboot2_tag_module *tag = (struct multiboot2_tag_module *)p;
			(void)hvld->fill_module_tag(hvld, tag, i);
			p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
		}
	}

	/* Memory map */
	{
		unsigned i;
		struct multiboot2_tag_mmap *tag = (struct multiboot2_tag_mmap *)p;
		struct multiboot2_mmap_entry *e;
		uint32_t mmap_count = mmap_info.map_size / mmap_info.desc_size;
		int32_t e820_count = 0;

		err = fill_e820(hvld, mmap, mmap_count, &e820_count);
		if (err != EFI_SUCCESS)
			goto out;

		tag->type = MULTIBOOT2_TAG_TYPE_MMAP;
		tag->size = sizeof(struct multiboot2_tag_mmap) + sizeof(struct multiboot2_mmap_entry) * e820_count;
		tag->entry_size = sizeof(struct multiboot2_mmap_entry);
		tag->entry_version = 0;

		for (i = 0, e = (struct multiboot2_mmap_entry *)tag->entries; i < e820_count; i++) {
			e->addr = mmap[i].mm_base_addr;
			e->len = mmap[i].mm_length;
			e->type = mmap[i].mm_type;
			e->zero = 0;
			e = (struct multiboot2_mmap_entry *)((char *)e + sizeof(struct multiboot2_mmap_entry));
		}

		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* ACPI new */
	{
		struct multiboot2_tag_new_acpi *tag = (struct multiboot2_tag_new_acpi *)p;
		tag->type = MULTIBOOT2_TAG_TYPE_ACPI_NEW;
		tag->size = sizeof(struct multiboot2_tag_new_acpi) + rsdp->length;
		memcpy((char *)tag->rsdp, (char *)rsdp, rsdp->length);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* EFI64 system table */
	{
		struct multiboot2_tag_efi64 *tag = (struct multiboot2_tag_efi64 *)p;
		tag->type = MULTIBOOT2_TAG_TYPE_EFI64;
		tag->size = sizeof(struct multiboot2_tag_efi64);
		tag->pointer = (uint64_t)sys_table;
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* EFI memory map */
	{
		struct multiboot2_tag_efi_mmap *tag = (struct multiboot2_tag_efi_mmap *)p;
		tag->type = MULTIBOOT2_TAG_TYPE_EFI_MMAP;
		tag->size = sizeof(struct multiboot2_tag_efi_mmap) + mmap_info.map_size;
		tag->descr_size = mmap_info.desc_size;
		tag->descr_vers = mmap_info.desc_version;
		memcpy((char *)tag->efi_mmap, (char *)mmap_info.mmap, mmap_info.map_size);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* END */
	{
		struct multiboot2_tag *tag = (struct multiboot2_tag *)p;
		tag->type = MULTIBOOT2_TAG_TYPE_END;
		tag->size = sizeof(struct multiboot2_tag);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	((uint32_t *)mbistart)[0] = (uint64_t)((char *)p - (char *)mbistart);
	((uint32_t *)mbistart)[1] = 0;

	*mbi_addr = (void *)mbistart;

	return EFI_SUCCESS;

out:
	free_pool(mbistart);
	return err;
}
#endif

static EFI_STATUS
run_acrn(EFI_HANDLE image, HV_LOADER hvld)
{
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS err;
	struct multiboot_mmap *mmap;
	struct multiboot_info *mbi;
	int32_t i;
	EFI_CONFIGURATION_TABLE *config_table;
	char *uefi_boot_loader_name;
	const char loader_name[BOOT_LOADER_NAME_SIZE] = UEFI_BOOT_LOADER_NAME;

	err = allocate_pool(EfiLoaderData, EFI_BOOT_MEM_SIZE, (VOID *)&addr);
	if (err != EFI_SUCCESS) {
		Print(L"Failed to allocate memory for EFI boot\n");
		goto out;
	}
	(void)memset((void *)addr, 0x0, EFI_BOOT_MEM_SIZE);

	mmap = MBOOT_MMAP_PTR(addr);
	mbi = MBOOT_INFO_PTR(addr);

	uefi_boot_loader_name = BOOT_LOADER_NAME_PTR(addr);
	memcpy(uefi_boot_loader_name, loader_name, BOOT_LOADER_NAME_SIZE);

	/* reserve secondary memory region for CPU trampoline code */
	err = emalloc_reserved_mem(&addr, CONFIG_LOW_RAM_SIZE, MEM_ADDR_1MB);
	if (err != EFI_SUCCESS)
		goto out;
	if (addr < 4096)
		Print(L"Warning: CPU trampoline code buf occupied zero-page\n");

	config_table = sys_table->ConfigurationTable;

	for (i = 0; i < sys_table->NumberOfTableEntries; i++) {
		EFI_GUID acpi_20_table_guid = ACPI_20_TABLE_GUID;
		EFI_GUID acpi_table_guid = ACPI_TABLE_GUID;

		if (CompareGuid(&acpi_20_table_guid,
			&config_table->VendorGuid) == 0) {
			rsdp = config_table->VendorTable;
			break;
		}

		if (CompareGuid(&acpi_table_guid,
			&config_table->VendorGuid) == 0)
			rsdp = config_table->VendorTable;

		config_table++;
	}

	if (rsdp == NULL) {
		Print(L"unable to find RSDP\n");
		goto out;
	}

#ifdef CONFIG_MULTIBOOT2
	/*
	 * If multiboot2, we're not going to use the mbi allocated above.
	 * construct_mbi2 will reallocate mbi and this value will be overwritten.
	 */
	err = construct_mbi2(hvld, (void **)&mbi, mmap);
#else
	err = get_current_memory_map(&mmap_info);
	if (err != EFI_SUCCESS)
		goto out;

	/* construct multiboot info and deliver it to hypervisor */
	err = construct_mbi(hvld, mbi, mmap, mods);
	if (err != EFI_SUCCESS)
		goto out;

	/* Set boot loader name in the multiboot header of UEFI, this name is used by hypervisor;
	 * The host physical start address of boot loader name is stored in multiboot header.
	 */
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_LOADER_NAME;
	mbi->mi_loader_name = (UINT32)uefi_boot_loader_name;

	mbi->mi_mods_addr  = mods_addr;
	mbi->mi_mods_count = mods_count;
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MODS;
#endif

	err = terminate_boot_services(image, mmap_info.map_key);
	if (err != EFI_SUCCESS)
		goto out;

	hv_jump(hvld->get_hv_hpa(hvld), mbi);

	/* Not reached on success */
out:
	return err;
}

EFI_STATUS reserve_unconfigure_high_memory(void)
{
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL
	UINTN map_size, map_key, desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	UINTN d, map_end;
	UINTN i;
	UINT32 desc_version;
	EFI_STATUS err;
	UINT64 reserved_hpa;
	EFI_PHYSICAL_ADDRESS top_addr_space = CONFIG_PLATFORM_RAM_SIZE + PLATFORM_LO_MMIO_SIZE;

	err = memory_map(&map_buf, &map_size, &map_key, &desc_size, &desc_version);
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

		if (end > top_addr_space) {
			if (start < top_addr_space)
				start = top_addr_space;
			err = emalloc_fixed_addr(&reserved_hpa, end - start, start);
			Print(L"memory region (%lx, %lx) is truncated from region (%lx, %lx).",
					start, end, desc->PhysicalStart, end);
			if (err != EFI_SUCCESS)
				break;
		}
	}

	free_pool(map_buf);
fail:
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

	INTN index;

	InitializeLib(image, _table);
	sys_table = _table;
	boot = sys_table->BootServices;

	if (CheckCrc(sys_table->Hdr.HeaderSize, &sys_table->Hdr) != TRUE)
		return EFI_LOAD_ERROR;

	/* make sure only bsp is enable before entering hv */
	enable_disable_all_ap(FALSE);

	err = handle_protocol(image, &LoadedImageProtocol, (void **)&info);
	if (err != EFI_SUCCESS)
		goto failed;

	err = reserve_unconfigure_high_memory();
	if (err != EFI_SUCCESS) {
		Print(L"Unable to reserve un-configure high memory %r ", err);
		goto failed;
	}

	/*
	 * Load hypervisor boot image handler. Currently Slim Bootloader
	 * compatible embedded container format is supported. File system
	 * mode to come future.
	 */
	err = container_init(info, &hvld);
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
