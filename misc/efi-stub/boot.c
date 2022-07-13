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
set_mor_bit()
{
	EFI_STATUS err;
	UINT32 attrs;
	UINTN size = 1;
	uint8_t data = 0;
	EFI_GUID efi_var_morctl_guid = EFI_VAR_MORCTL_GUID;
#ifdef MORCTRL_LOCK_ENABLED
	EFI_GUID efi_var_morctllock_guid = EFI_VAR_MORCTLLOCK_GUID;
#endif

	/*
	 * Per TCG Platform Reset Attack Mitigation Spec 1.10 rev 17, Chp 4.1
	 * MORCtrl is a 1-byte unsigned number and should be created by the firmware.
	 */
	err = get_variable(EFI_VAR_MORCTL_NAME, &efi_var_morctl_guid, &attrs, &size, (void *)&data);
	if (err != EFI_SUCCESS) {
		if (err == EFI_BUFFER_TOO_SMALL) {
			Print(L"Wrong MORCtrl variable size: 0x%x byte, should be 1 byte\n", size);
		} else if (err == EFI_NOT_FOUND) {
			Print(L"Warning: MORCtrl variable not found\n");
		}

		goto out;
	}

	if (attrs != (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)) {
		Print(L"Wrong MORCtrl attributes: 0x%x\n", attrs);
		goto out;
	}

	/* Bit 0 set: Firmware MUST set the MOR bit */
	/* Bit 4 cleared: Firmware MAY autodetect a clean shutdown of the Static RTM OS. */
	data = 0x1;

	err = set_variable(EFI_VAR_MORCTL_NAME, &efi_var_morctl_guid, attrs, size, &data);
	if (err != EFI_SUCCESS)
		goto out;

#ifdef MORCTRL_LOCK_ENABLED
	/*
	 * MORCTRL_LOCK_ENABLED is NOT part of the board configuration.
	 * To activate MORCTRL_LOCK_ENABLED, manually add -DMORCTRL_LOCK_ENABLED to the CFLAGS.
	 */

	/* Lock MORCtrl with MORCtrlLock */
	size = 1;
	err = get_variable(EFI_VAR_MORCTLLOCK_NAME, &efi_var_morctllock_guid, &attrs, &size, (void *)&data);
	if (err != EFI_SUCCESS)
		goto out;

	if (attrs != (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)) {
		Print(L"Wrong MORCtrlLock attributes: 0x%x\n", attrs);
		goto out;
	}

	if (data == 0x1 || data == 0x2) {
		Print(L"Warning: MORCtrl already locked. No locking operation performed.\n");
		goto out;
	}

	/*
	 * Input value 1, size 1: Lock without key
	 * Try to lock MemoryOverwriteRequestControlLock and MemoryOverwriteRequestControl
	 * If success, MORCtrl and MORCtrlLock will be read-only until next boot, and reboot
	 * is the only way to unlock these variables.
	 */
	data = 0x1;
	size = 0x1;
	err = set_variable(EFI_VAR_MORCTLLOCK_NAME, &efi_var_morctllock_guid, attrs, size, (void *)&data);
	if (err != EFI_SUCCESS)
		goto out;
#endif
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

static EFI_STATUS
fill_e820(HV_LOADER hvld, struct efi_memmap_info *mmap_info,
	struct multiboot_mmap *mmap, int32_t *e820_count)
{
	EFI_STATUS err = EFI_SUCCESS;
	uint32_t mmap_entry_count = mmap_info->map_size / mmap_info->desc_size;
	int32_t i, j;

	/*
	 * Convert the EFI memory map to E820.
	 */
	for (i = 0, j = 0; i < mmap_entry_count && j < MBOOT_MMAP_NUMS - 1; i++) {
		EFI_MEMORY_DESCRIPTOR *d;
		uint32_t e820_type = 0;

		d = (EFI_MEMORY_DESCRIPTOR *)((uint64_t)mmap_info->mmap + \
			(i * mmap_info->desc_size));
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
	mmap[j].mm_length = hvld->get_hv_ram_size(hvld);
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


EFI_STATUS construct_mbi(HV_LOADER hvld, struct multiboot_info **mbinfo, struct efi_memmap_info *mmap_info)
{
	EFI_STATUS err = EFI_SUCCESS;
	int32_t e820_count = 0;
	EFI_PHYSICAL_ADDRESS addr;
	struct multiboot_mmap *mmap;
	struct multiboot_info *mbi;
	char *uefi_boot_loader_name;
	static const char loader_name[BOOT_LOADER_NAME_SIZE] = UEFI_BOOT_LOADER_NAME;

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

	err = get_efi_memmap(mmap_info, 0);
	if (err != EFI_SUCCESS)
		goto out;

	err = fill_e820(hvld, mmap_info, mmap, &e820_count);
	if (err != EFI_SUCCESS)
		goto out;

	mbi->mi_cmdline = (UINTN)hvld->get_boot_cmd(hvld);
	mbi->mi_mmap_addr = (UINTN)mmap;
	mbi->mi_mmap_length = e820_count*sizeof(struct multiboot_mmap);
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP | MULTIBOOT_INFO_HAS_CMDLINE;

	/* Set boot loader name in the multiboot header of UEFI, this name is used by hypervisor;
	 * The host physical start address of boot loader name is stored in multiboot header.
	 */
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_LOADER_NAME;
	mbi->mi_loader_name = (UINT32)(uint64_t)uefi_boot_loader_name;

	mbi->mi_mods_addr  = hvld->get_mod_hpa(hvld);
	mbi->mi_mods_count = hvld->get_mod_count(hvld);
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MODS;

	*mbinfo = mbi;
out:
	return err;
}

static struct acpi_table_rsdp *
search_rsdp()
{
	unsigned i;
	struct acpi_table_rsdp *rsdp = NULL;
	EFI_CONFIGURATION_TABLE *config_table = sys_table->ConfigurationTable;

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

	return rsdp;
}

static uint32_t
get_mbi2_size(HV_LOADER hvld, struct efi_memmap_info *mmap_info, uint32_t rsdp_length)
{
	uint32_t mmap_entry_count = mmap_info->map_size / mmap_info->desc_size;

	return 2 * sizeof(uint32_t) \
		/* Boot command line */
		+ (sizeof(struct multiboot2_tag_string) + \
			ALIGN_UP(hvld->get_boot_cmdsize(hvld), MULTIBOOT2_TAG_ALIGN)) \

		/* Boot loader name */
		+ (sizeof(struct multiboot2_tag_string) + \
			ALIGN_UP(BOOT_LOADER_NAME_SIZE, MULTIBOOT2_TAG_ALIGN)) \

		/* Modules */
		+ (hvld->get_mod_count(hvld) * sizeof(struct multiboot2_tag_module) + \
			hvld->get_total_modcmdsize(hvld)) \

		/* Memory Map */
		+ ALIGN_UP((sizeof(struct multiboot2_tag_mmap) + \
			mmap_entry_count * sizeof(struct multiboot2_mmap_entry)), MULTIBOOT2_TAG_ALIGN) \

		/* ACPI new */
		+ ALIGN_UP(sizeof(struct multiboot2_tag_new_acpi) + \
			rsdp_length, MULTIBOOT2_TAG_ALIGN) \

		/* EFI64 system table */
		+ ALIGN_UP(sizeof(struct multiboot2_tag_efi64), MULTIBOOT2_TAG_ALIGN) \

		/* EFI memmap: Add an extra page since UEFI can alter the memory map */
		+ ALIGN_UP(sizeof(struct multiboot2_tag_efi_mmap) + \
			ALIGN_UP(mmap_info->map_size + 0x1000, 0x1000), MULTIBOOT2_TAG_ALIGN) \

		/* END */
		+ sizeof(struct multiboot2_tag);
}

EFI_STATUS
construct_mbi2(struct hv_loader *hvld, void **mbi_addr, struct efi_memmap_info *mmap_info)
{
	uint64_t *mbistart;
	uint64_t *p;
	uint32_t mbi2_size;
	struct multiboot_mmap *mmap;
	struct acpi_table_rsdp *rsdp;
	EFI_STATUS err;

	rsdp = search_rsdp();
	if (!rsdp)
		return EFI_NOT_FOUND;

	/* Get size only for mbi size calculation */
	err = get_efi_memmap(mmap_info, 1);
	if (err != EFI_SUCCESS && err != EFI_BUFFER_TOO_SMALL)
		return err;

	mbi2_size = get_mbi2_size(hvld, mmap_info, rsdp->length);

	/* per UEFI spec v2.9: This allocation is guaranteed to be 8-bytes aligned */
	err = allocate_pool(EfiLoaderData, mbi2_size, (void **)&mbistart);
	if (err != EFI_SUCCESS)
		goto out;

	memset(mbistart, 0x0, mbi2_size);

	/* Allocate temp buffer to hold memory map */
	err = allocate_pool(EfiLoaderData,
		(mmap_info->map_size / mmap_info->desc_size) * sizeof(struct multiboot_mmap),
		(void **)&mmap);
	if (err != EFI_SUCCESS)
		goto out;

	/*
	 * Get full memory map again.
	 * We have just allocated memory and the mmap_info will be different.
	 */
	err = get_efi_memmap(mmap_info, 0);
	if (err != EFI_SUCCESS)
		goto out;

	/* total_size and reserved */
	p = mbistart;
	p += (2 * sizeof(uint32_t)) / sizeof(uint64_t);

	/* Boot command line */
	{
		struct multiboot2_tag_string *tag = (struct multiboot2_tag_string *)p;
		UINTN cmdline_size = hvld->get_boot_cmdsize(hvld);
		tag->type = MULTIBOOT2_TAG_TYPE_CMDLINE;
		tag->size = sizeof(struct multiboot2_tag_string) + cmdline_size;
		memcpy(tag->string, hvld->get_boot_cmd(hvld), cmdline_size);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* Boot loader name */
	{
		struct multiboot2_tag_string *tag = (struct multiboot2_tag_string *)p;
		tag->type = MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME;
		tag->size = sizeof(struct multiboot2_tag_string) + BOOT_LOADER_NAME_SIZE;
		memcpy(tag->string, UEFI_BOOT_LOADER_NAME, BOOT_LOADER_NAME_SIZE);
		p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
	}

	/* Modules */
	{
		unsigned i;
		uint32_t mod_count = hvld->get_mod_count(hvld);
		for (i = 0; i < mod_count; i++) {
			struct multiboot2_tag_module *tag = (struct multiboot2_tag_module *)p;
			MB_MODULE_INFO *modinfo = hvld->get_mods_info(hvld, i);
			tag->type = MULTIBOOT2_TAG_TYPE_MODULE;
			tag->size = sizeof(struct multiboot2_tag_module) + modinfo->cmdsize;
			tag->mod_start = modinfo->mod_start;
			tag->mod_end = modinfo->mod_end;
			memcpy(tag->cmdline, modinfo->cmd, modinfo->cmdsize);
			p += ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / sizeof(uint64_t);
		}
	}

	/* Memory map */
	{
		unsigned i;
		struct multiboot2_tag_mmap *tag = (struct multiboot2_tag_mmap *)p;
		struct multiboot2_mmap_entry *e;
		int32_t e820_count = 0;

		err = fill_e820(hvld, mmap_info, mmap, &e820_count);
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
		tag->size = sizeof(struct multiboot2_tag_efi_mmap) + mmap_info->map_size;
		tag->descr_size = mmap_info->desc_size;
		tag->descr_vers = mmap_info->desc_version;
		memcpy((char *)tag->efi_mmap, (char *)mmap_info->mmap, mmap_info->map_size);
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

static EFI_STATUS
run_acrn(EFI_HANDLE image, HV_LOADER hvld)
{
	EFI_STATUS err;
	struct efi_memmap_info memmapinfo;
	void *mbi;
	int32_t mb_version = hvld->get_multiboot_version(hvld);

	err = set_mor_bit();
	/* If MOR not supported, emit a warning and proceed */
	if (err != EFI_SUCCESS && err != EFI_NOT_FOUND)
		goto out;

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
