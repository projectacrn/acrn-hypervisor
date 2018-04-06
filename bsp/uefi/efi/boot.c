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

EFI_SYSTEM_TABLE *sys_table;
EFI_BOOT_SERVICES *boot;
EFI_RUNTIME_SERVICES *runtime;
extern const uint64_t guest_entry;

static inline void hv_jump(EFI_PHYSICAL_ADDRESS hv_start,
			struct multiboot_info *mbi, struct efi_ctx *efi_ctx)
{
	hv_func hf;

	efi_ctx->rip = (uint64_t)&guest_entry;

	/* The 64-bit entry of acrn hypervisor is 0x200 from the start
	 * address of hv image. But due to there is multiboot header,
	 * so it has to be added with 0x10.
	 *
	 * FIXME: The hardcode value 0x210 should be worked out
	 * from the link address of cpu_primary_start_64 in acrn.out
	 */
	hf = (hv_func)(hv_start + 0x210);

	asm volatile ("cli");

	/* jump to acrn hypervisor */
	hf(MULTIBOOT_INFO_MAGIC, mbi);
}

EFI_STATUS
construct_mbi(struct multiboot_info **mbi_ret, struct efi_ctx *efi_ctx)
{
	UINTN map_size, _map_size, map_key;
	UINT32 desc_version;
	UINTN desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS err = EFI_SUCCESS;
	struct multiboot_info *mbi;
	struct multiboot_mmap *mmap;
	int i, j;

	/* multiboot info */
	err = emalloc(16384, 8, &addr);
	if (err != EFI_SUCCESS)
		goto out;

	mbi = (struct multiboot_info *)(UINTN)addr;
	memset((void *)mbi, 0x0, sizeof(*mbi));

	/* allocate mmap[] */
	err = emalloc(sizeof(struct multiboot_mmap)*128, 8, &addr);
	if (err != EFI_SUCCESS)
		goto out;
	mmap = (struct multiboot_mmap *)(UINTN)addr;
	memset((void *)mmap, 0x0, sizeof(*mmap)*128);

	/* We're just interested in the map's size for now */
	map_size = 0;
	err = get_memory_map(&map_size, NULL, NULL, NULL, NULL);
	if (err != EFI_SUCCESS && err != EFI_BUFFER_TOO_SMALL)
		goto out;

again:
	_map_size = map_size;
	err = emalloc(map_size, 1, &addr);
	if (err != EFI_SUCCESS)
		goto out;
	map_buf = (EFI_MEMORY_DESCRIPTOR *)(UINTN)addr;

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
			efree((UINTN)map_buf, _map_size);
			goto again;
		}
		goto out;
	}

	/*
	 * Convert the EFI memory map to E820.
	 */
	for (i = 0, j = 0; i < map_size / desc_size; i++) {
		EFI_MEMORY_DESCRIPTOR *d;
		unsigned int e820_type = 0;

		d = (EFI_MEMORY_DESCRIPTOR *)((unsigned long)map_buf + (i * desc_size));
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

		if (j && mmap[j-1].mm_type == e820_type &&
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

	/* switch hv memory region(0x20000000 ~ 0x22000000) to
	 * available RAM in e820 table
	 */
	mmap[j].mm_base_addr = CONFIG_RAM_START;
	mmap[j].mm_length = CONFIG_RAM_SIZE;
	mmap[j].mm_type = E820_RAM;
	j++;

	/* reserve secondary memory region(0x1000 ~ 0x10000) for hv */
	err = __emalloc(CONFIG_LOW_RAM_SIZE, CONFIG_LOW_RAM_START,
		&addr, EfiReservedMemoryType);
	if (err != EFI_SUCCESS)
		goto out;

	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP | MULTIBOOT_INFO_HAS_CMDLINE;
	mbi->mi_mmap_length = j*sizeof(struct multiboot_mmap);

	//mbi->mi_cmdline = (UINTN)"uart=mmio@0x92230000";
	//mbi->mi_cmdline = (UINTN)"uart=port@0x3F8";
	mbi->mi_cmdline = (UINTN)"uart=disabled";
	mbi->mi_mmap_addr = (UINTN)mmap;

	mbi->mi_flags |= MULTIBOOT_INFO_HAS_DRIVES;
	mbi->mi_drives_addr = (UINT32)(UINTN)efi_ctx;

	*mbi_ret = mbi;
out:
	return err;
}

static EFI_STATUS
switch_to_guest_mode(EFI_HANDLE image)
{
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS err;
	struct multiboot_info *mbi = NULL;
	struct efi_ctx *efi_ctx;
	struct acpi_table_rsdp *rsdp = NULL;
	int i;
	EFI_CONFIGURATION_TABLE *config_table;

	err = emalloc(sizeof(struct efi_ctx), 8, &addr);
	if (err != EFI_SUCCESS)
		goto out;

	efi_ctx = (struct efi_ctx *)(UINTN)addr;

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

	if (!rsdp) {
		Print(L"unable to find RSDP\n");
		goto out;
	}

	efi_ctx->rsdp = rsdp;

	/* construct multiboot info and deliver it to hypervisor */
	err = construct_mbi(&mbi, efi_ctx);
	if (err != EFI_SUCCESS)
		goto out;

	asm volatile ("mov %%cr0, %0" : "=r"(efi_ctx->cr0));
	asm volatile ("mov %%cr3, %0" : "=r"(efi_ctx->cr3));
	asm volatile ("mov %%cr4, %0" : "=r"(efi_ctx->cr4));
	asm volatile ("sidt %0" :: "m" (efi_ctx->idt));
	asm volatile ("sgdt %0" :: "m" (efi_ctx->gdt));
	asm volatile ("str %0" :: "m" (efi_ctx->tr_sel));
	asm volatile ("sldt %0" :: "m" (efi_ctx->ldt_sel));

	asm volatile ("mov %%cs, %%ax" : "=a"(efi_ctx->cs_sel));
	asm volatile ("lar %%eax, %%eax"
					: "=a"(efi_ctx->cs_ar)
					: "a"(efi_ctx->cs_sel)
					);
	efi_ctx->cs_ar = (efi_ctx->cs_ar >> 8) & 0xf0ff; /* clear bits 11:8 */

	asm volatile ("mov %%es, %%ax" : "=a"(efi_ctx->es_sel));
	asm volatile ("mov %%ss, %%ax" : "=a"(efi_ctx->ss_sel));
	asm volatile ("mov %%ds, %%ax" : "=a"(efi_ctx->ds_sel));
	asm volatile ("mov %%fs, %%ax" : "=a"(efi_ctx->fs_sel));
	asm volatile ("mov %%gs, %%ax" : "=a"(efi_ctx->gs_sel));

	uint32_t idx = 0xC0000080; /* MSR_IA32_EFER */
	uint32_t msrl, msrh;
	asm volatile ("rdmsr" : "=a"(msrl), "=d"(msrh) : "c"(idx));
	efi_ctx->efer = ((uint64_t)msrh<<32) | msrl;

	asm volatile ("pushf\n\t"
					"pop %0\n\t"
					: "=r"(efi_ctx->rflags)
					: );

	asm volatile ("movq %%rax, %0" : "=r"(efi_ctx->rax));
	asm volatile ("movq %%rbx, %0" : "=r"(efi_ctx->rbx));
	asm volatile ("movq %%rcx, %0" : "=r"(efi_ctx->rcx));
	asm volatile ("movq %%rdx, %0" : "=r"(efi_ctx->rdx));
	asm volatile ("movq %%rdi, %0" : "=r"(efi_ctx->rdi));
	asm volatile ("movq %%rsi, %0" : "=r"(efi_ctx->rsi));
	asm volatile ("movq %%rsp, %0" : "=r"(efi_ctx->rsp));
	asm volatile ("movq %%rbp, %0" : "=r"(efi_ctx->rbp));
	asm volatile ("movq %%r8, %0" : "=r"(efi_ctx->r8));
	asm volatile ("movq %%r9, %0" : "=r"(efi_ctx->r9));
	asm volatile ("movq %%r10, %0" : "=r"(efi_ctx->r10));
	asm volatile ("movq %%r11, %0" : "=r"(efi_ctx->r11));
	asm volatile ("movq %%r12, %0" : "=r"(efi_ctx->r12));
	asm volatile ("movq %%r13, %0" : "=r"(efi_ctx->r13));
	asm volatile ("movq %%r14, %0" : "=r"(efi_ctx->r14));
	asm volatile ("movq %%r15, %0" : "=r"(efi_ctx->r15));

	hv_jump(CONFIG_RAM_START, mbi, efi_ctx);
	asm volatile (".global guest_entry\n\t"
				  "guest_entry:\n\t");

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
	EFI_PHYSICAL_ADDRESS addr;
	UINTN sec_addr;
	UINTN sec_size;
	char *section;
	EFI_DEVICE_PATH *path;
	CHAR16 *bootloader_name;
	CHAR16 *bootloader_name_with_path;
	EFI_HANDLE bootloader_image;

	InitializeLib(image, _table);

	sys_table = _table;
	boot = sys_table->BootServices;
	runtime = sys_table->RuntimeServices;

	if (CheckCrc(sys_table->Hdr.HeaderSize, &sys_table->Hdr) != TRUE)
		return EFI_LOAD_ERROR;


	err = handle_protocol(image, &LoadedImageProtocol, (void **)&info);
	if (err != EFI_SUCCESS)
		goto failed;

	section = ".hv";
	err = get_pe_section(info->ImageBase, section, &sec_addr, &sec_size);
	if (EFI_ERROR(err)) {
		Print(L"Unable to locate section of ACRNHV %r ", err);
		goto failed;
	}

	err = __emalloc(CONFIG_RAM_SIZE, CONFIG_RAM_START, &addr,
		EfiReservedMemoryType);
	if (err != EFI_SUCCESS)
		goto failed;

	/* Copy ACRNHV binary to fixed phys addr. LoadImage and StartImage ?? */
	memcpy((char*)addr, info->ImageBase + sec_addr, sec_size);

	/* load hypervisor and begin to run on it */
	err = switch_to_guest_mode(image);

	if (err != EFI_SUCCESS)
		goto failed;

	/* load and start the default bootloader */
	bootloader_name = ch8_2_ch16(CONFIG_UEFI_OS_LOADER_NAME);
	bootloader_name_with_path =
		PoolPrint(L"%s%s", L"\\EFI\\BOOT\\", bootloader_name);
	path = FileDevicePath(info->DeviceHandle, bootloader_name_with_path);
	if (!path)
		goto free_args;

	FreePool(bootloader_name);

	err = uefi_call_wrapper(boot->LoadImage, 6, FALSE, image,
		path, NULL, 0, &bootloader_image);
	if (EFI_ERROR(err)) {
		uefi_call_wrapper(boot->Stall, 1, 3 * 1000 * 1000);
		goto failed;
	}

	err = uefi_call_wrapper(boot->StartImage, 3, bootloader_image,
		NULL, NULL);
	if (EFI_ERROR(err)) {
		uefi_call_wrapper(boot->Stall, 1, 3 * 1000 * 1000);
		goto failed;
	}
	uefi_call_wrapper(boot->UnloadImage, 1, bootloader_image);

	return EFI_SUCCESS;

free_args:
	FreePool(bootloader_name);
failed:
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
	return exit(image, err, ERROR_STRING_LENGTH, error_buf);
}

