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
#include "uefi.h"

EFI_SYSTEM_TABLE *sys_table;
EFI_BOOT_SERVICES *boot;
char *cmdline = NULL;
extern const uint64_t guest_entry;
static UINT64 hv_hpa;

static inline void hv_jump(EFI_PHYSICAL_ADDRESS hv_start,
			struct multiboot_info *mbi, struct efi_context *efi_ctx)
{
	hv_func hf;

	efi_ctx->vcpu_regs.rip = (uint64_t)&guest_entry;

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

EFI_STATUS construct_mbi(EFI_PHYSICAL_ADDRESS hv_hpa, struct multiboot_info *mbi,
		struct multiboot_mmap *mmap)
{
	UINTN map_size, map_key;
	UINT32 desc_version;
	UINTN desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	EFI_STATUS err = EFI_SUCCESS;
	int32_t i, j, mmap_entry_count;

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

	mmap_entry_count = map_size / desc_size;
	/*
	 * Convert the EFI memory map to E820.
	 */
	for (i = 0, j = 0; i < mmap_entry_count && j < MBOOT_MMAP_NUMS - 1; i++) {
		EFI_MEMORY_DESCRIPTOR *d;
		uint32_t e820_type = 0;

		d = (EFI_MEMORY_DESCRIPTOR *)((uint64_t)map_buf + (i * desc_size));
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
	mmap[j].mm_base_addr = hv_hpa;
	mmap[j].mm_length = CONFIG_HV_RAM_SIZE;
	mmap[j].mm_type = E820_RAM;
	j++;

	mbi->mi_cmdline = (UINTN)cmdline;
	mbi->mi_mmap_addr = (UINTN)mmap;
	mbi->mi_mmap_length = j*sizeof(struct multiboot_mmap);
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP | MULTIBOOT_INFO_HAS_CMDLINE;
out:
	return err;
}

static EFI_STATUS
switch_to_guest_mode(EFI_HANDLE image, EFI_PHYSICAL_ADDRESS hv_hpa)
{
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS err;
	struct multiboot_mmap *mmap;
	struct multiboot_info *mbi;
	struct efi_context *efi_ctx;
	struct acpi_table_rsdp *rsdp = NULL;
	int32_t i;
	EFI_CONFIGURATION_TABLE *config_table;

	err = allocate_pool(EfiLoaderData, EFI_BOOT_MEM_SIZE, (VOID *)&addr);
	if (err != EFI_SUCCESS) {
		Print(L"Failed to allocate memory for EFI boot\n");
		goto out;
	}
	(void)memset((void *)addr, 0x0, EFI_BOOT_MEM_SIZE);

	mmap = MBOOT_MMAP_PTR(addr);
	mbi = MBOOT_INFO_PTR(addr);
	efi_ctx = BOOT_CTX_PTR(addr);

	/* reserve secondary memory region for CPU trampoline code */
	err = emalloc_reserved_mem(&addr, CONFIG_LOW_RAM_SIZE, MEM_ADDR_1MB);
	if (err != EFI_SUCCESS)
		goto out;
	if (addr < 4096)
		Print(L"Warning: CPU trampoline code buf occupied zero-page\n");

	efi_ctx->ap_trampoline_buf = (void *)addr;

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

	efi_ctx->rsdp = rsdp;

	/* construct multiboot info and deliver it to hypervisor */
	err = construct_mbi(hv_hpa, mbi, mmap);
	if (err != EFI_SUCCESS)
		goto out;

	mbi->mi_flags |= MULTIBOOT_INFO_HAS_DRIVES;
	mbi->mi_drives_addr = (UINT32)(UINTN)efi_ctx;

	asm volatile ("pushf\n\t"
		      "pop %0\n\t"
		      : "=r"(efi_ctx->vcpu_regs.rflags)
		      : );
	asm volatile ("movq %%rax, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rax));
	asm volatile ("movq %%rbx, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rbx));
	asm volatile ("movq %%rcx, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rcx));
	asm volatile ("movq %%rdx, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rdx));
	asm volatile ("movq %%rdi, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rdi));
	asm volatile ("movq %%rsi, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rsi));
	asm volatile ("movq %%rsp, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rsp));
	asm volatile ("movq %%rbp, %0" : "=r"(efi_ctx->vcpu_regs.gprs.rbp));
	asm volatile ("movq %%r8,  %0" : "=r"(efi_ctx->vcpu_regs.gprs.r8));
	asm volatile ("movq %%r9,  %0" : "=r"(efi_ctx->vcpu_regs.gprs.r9));
	asm volatile ("movq %%r10, %0" : "=r"(efi_ctx->vcpu_regs.gprs.r10));
	asm volatile ("movq %%r11, %0" : "=r"(efi_ctx->vcpu_regs.gprs.r11));
	asm volatile ("movq %%r12, %0" : "=r"(efi_ctx->vcpu_regs.gprs.r12));
	asm volatile ("movq %%r13, %0" : "=r"(efi_ctx->vcpu_regs.gprs.r13));
	asm volatile ("movq %%r14, %0" : "=r"(efi_ctx->vcpu_regs.gprs.r14));
	asm volatile ("movq %%r15, %0" : "=r"(efi_ctx->vcpu_regs.gprs.r15));

	hv_jump(hv_hpa, mbi, efi_ctx);
	asm volatile (".global guest_entry\n\t"
				  "guest_entry:\n\t");

out:
	return err;
}

static inline EFI_STATUS isspace(CHAR8 ch)
{
    return ((uint8_t)ch <= ' ');
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
	UINTN sec_addr;
	UINTN sec_size;
	char *section;
	EFI_DEVICE_PATH *path;

	INTN i, index;
	CHAR16 *bootloader_name = NULL;
	CHAR16 bootloader_param[] = L"bootloader=";
	EFI_HANDLE bootloader_image;
	CHAR16 *options = NULL;
	UINT32 options_size = 0;
	CHAR16 *cmdline16, *n;

	InitializeLib(image, _table);
	sys_table = _table;
	boot = sys_table->BootServices;

	if (CheckCrc(sys_table->Hdr.HeaderSize, &sys_table->Hdr) != TRUE)
		return EFI_LOAD_ERROR;

	err = handle_protocol(image, &LoadedImageProtocol, (void **)&info);
	if (err != EFI_SUCCESS)
		goto failed;

	/* get the options */
	options = info->LoadOptions;
	options_size = info->LoadOptionsSize;

	/* convert the options to cmdline */
	if (options_size > 0)
		cmdline = ch16_2_ch8(options);

	/* First check if we were given a bootloader name
	 * E.g.: "bootloader=\EFI\org.clearlinux\bootloaderx64.efi"
	 */
	cmdline16 = StrDuplicate(options);
	bootloader_name = strstr_16(cmdline16, bootloader_param);
	if (bootloader_name) {
		bootloader_name = bootloader_name + StrLen(bootloader_param);
		n = bootloader_name;
		i = 0;
		while (*n && !isspace((CHAR8)*n) && (*n < 0xff)) {
			n++; i++;
		}
		*n++ = '\0';
	} else {
		/*
		 * If we reach this point, it means we did not receive a specific
		 * bootloader name to be used. Fall back to the default bootloader
		 * as specified in config.h
		 */
		bootloader_name = ch8_2_ch16(CONFIG_UEFI_OS_LOADER_NAME);
	}

	section = ".hv";
	err = get_pe_section(info->ImageBase, section, &sec_addr, &sec_size);
	if (EFI_ERROR(err)) {
		Print(L"Unable to locate section of ACRNHV %r ", err);
		goto failed;
	}

	/* without relocateion enabled, hypervisor binary need to reside in
	 * fixed memory address starting from CONFIG_HV_RAM_START, make a call
	 * to emalloc_fixed_addr for that case. With CONFIG_RELOC enabled,
	 * hypervisor is able to do relocation, the only requirement is that
	 * it need to reside in memory below 4GB, call emalloc_reserved_mem()
	 * instead.
	 */
#ifdef CONFIG_RELOC
	err = emalloc_reserved_aligned(&hv_hpa, CONFIG_HV_RAM_SIZE, 1 << 21, MEM_ADDR_4GB);
#else
	err = emalloc_fixed_addr(&hv_hpa, CONFIG_HV_RAM_SIZE, CONFIG_HV_RAM_START);
#endif
	if (err != EFI_SUCCESS)
		goto failed;

	memcpy((char *)hv_hpa, info->ImageBase + sec_addr, sec_size);

	/* load hypervisor and begin to run on it */
	err = switch_to_guest_mode(image, hv_hpa);
	if (err != EFI_SUCCESS)
		goto failed;

	/* load and start the default bootloader */
	path = FileDevicePath(info->DeviceHandle, bootloader_name);
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

	/* If we don't wait for user input, (s)he will not see the error message */
        uefi_call_wrapper(sys_table->ConOut->OutputString, 2, sys_table->ConOut, \
                        L"\r\n\r\n\r\nHit any key to exit\r\n");
        uefi_call_wrapper(sys_table->BootServices->WaitForEvent, 3, 1, \
                        &sys_table->ConIn->WaitForKey, &index);

	return exit(image, err, ERROR_STRING_LENGTH, error_buf);
}
