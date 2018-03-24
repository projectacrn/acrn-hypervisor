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
#include "multiboot.h"

#define ERROR_STRING_LENGTH	32
#define EFI_LOADER_SIGNATURE    "EL64"

#define ACPI_XSDT_ENTRY_SIZE        (sizeof (UINT64))
#define ACPI_NAME_SIZE                  4
#define ACPI_OEM_ID_SIZE                6
#define ACPI_OEM_TABLE_ID_SIZE          8

EFI_SYSTEM_TABLE *sys_table;
EFI_BOOT_SERVICES *boot;
EFI_RUNTIME_SERVICES *runtime;

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

static inline BOOLEAN isspace(CHAR8 ch)
{
	return ((unsigned char)ch <= ' ');
}

#if 0
static void print_ch(char *str)
{
	int j;
	CHAR16 *buf;
	int len = strlen(str);

	buf = malloc((strlen(str) + 1)* 2);
	for (j=0; j<len; j++)
		buf[j] = str[j];
	buf[j] = 0;
	Print(L"CHAR16:::  %s\n", buf);
	free(buf);
}
#endif




struct acpi_table_rsdp {
    char signature[8];  /* ACPI signature, contains "RSD PTR " */
    UINT8 checksum;        /* ACPI 1.0 checksum */
    char oem_id[ACPI_OEM_ID_SIZE];  /* OEM identification */
    UINT8 revision;        /* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
    UINT32 rsdt_physical_address;  /* 32-bit physical address of the RSDT */
    UINT32 length;     /* Table length in bytes, including header (ACPI 2.0+) */
    UINT64 xsdt_physical_address;  /* 64-bit physical address of the XSDT (ACPI 2.0+) */
    UINT8 extended_checksum;   /* Checksum of entire table (ACPI 2.0+) */
    UINT8 reserved[3];     /* Reserved, must be zero */
};

struct acpi_table_header {
    char signature[ACPI_NAME_SIZE]; /* ASCII table signature */
    UINT32 length;     /* Length of table in bytes, including this header */
    UINT8 revision;        /* ACPI Specification minor version number */
    UINT8 checksum;        /* To make sum of entire table == 0 */
    char oem_id[ACPI_OEM_ID_SIZE];  /* ASCII OEM identification */
    char oem_table_id[ACPI_OEM_TABLE_ID_SIZE];  /* ASCII OEM table identification */
    UINT32 oem_revision;   /* OEM revision number */
    char asl_compiler_id[ACPI_NAME_SIZE];   /* ASCII ASL compiler vendor ID */
    UINT32 asl_compiler_revision;  /* ASL compiler version */
};

typedef void(*hv_func)(int, struct multiboot_info*, struct efi_ctx*);
EFI_IMAGE_ENTRY_POINT get_pe_entry(CHAR8 *base);

static inline void hv_jump(EFI_PHYSICAL_ADDRESS hv_start,
			struct multiboot_info* mbi, struct efi_ctx* pe)
{
	hv_func hf;

	asm volatile ("cli");

	/* The 64-bit kernel entry is 512 bytes after the start. */
	hf = (hv_func)(hv_start + 0x200);

        /*
         * The first parameter is a dummy because the kernel expects
         * boot_params in %[re]si.
         */
	hf(MULTIBOOT_INFO_MAGIC, mbi, pe);
}



EFI_STATUS get_path(CHAR16* name, EFI_LOADED_IMAGE *info, EFI_DEVICE_PATH **path)
{
	unsigned int pathlen;
	EFI_STATUS efi_status = EFI_SUCCESS;
	CHAR16 *pathstr, *pathname;
	int i;

	for (i = 0; i < StrLen(name); i++) {
		if (name[i] == '/')
			name[i] = '\\';
	}

	pathstr = DevicePathToStr(info->FilePath);
	for (i = 0; i < StrLen(pathstr); i++) {
		if (pathstr[i] == '/')
			pathstr[i] = '\\';
	}

	pathlen = StrLen(pathstr);

	if (name[0] == '\\') {
		*path = FileDevicePath(info->DeviceHandle, name);
		goto out;
	}

	for (i=pathlen - 1; i > 0; i--) {
		if (pathstr[i] == '\\') break;
	}
	pathstr[i] = '\0';

	pathlen = StrLen(pathstr);

	pathlen++;
	pathname = AllocatePool((pathlen + 1 + StrLen(name))*sizeof(CHAR16)); 
	if (!pathname) {
		Print(L"Failed to allocate memory for pathname\n");
		efi_status = EFI_OUT_OF_RESOURCES;
		goto out;
	}
	StrCpy(pathname, pathstr);
	StrCat(pathname, L"\\");
	StrCat(pathname, name);

	*path = FileDevicePath(info->DeviceHandle, pathname);
	
out:
	FreePool(pathstr);
	return efi_status;
}
/**
 * load_kernel - Load a kernel image into memory from the boot device
 */
EFI_STATUS
load_sos_image(EFI_HANDLE image, CHAR16 *name, CHAR16 *cmdline)
{
	UINTN map_size, _map_size, map_key;
	UINT32 desc_version;
	UINTN desc_size;
	EFI_MEMORY_DESCRIPTOR *map_buf;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_LOADED_IMAGE *info = NULL;
	EFI_STATUS err;
	struct multiboot_mmap *mmap;
	struct multiboot_info *mbi;

	struct acpi_table_rsdp *rsdp = NULL;
	int i, j;


	err = handle_protocol(image, &LoadedImageProtocol, (void **)&info);
	if (err != EFI_SUCCESS)
		goto out;


	EFI_HANDLE bz_hd;
	EFI_DEVICE_PATH *path;
	EFI_LOADED_IMAGE *bz_info = NULL;
	EFI_IMAGE_ENTRY_POINT pe_entry;
	struct efi_ctx* pe;

	err = get_path(name, info, &path);
	if (err != EFI_SUCCESS) {
		Print(L"fail to get bzImage.efi path");
		goto out;
	}

	err = uefi_call_wrapper(BS->LoadImage, 6, FALSE, image, path, NULL, 0, &bz_hd);

	if (err != EFI_SUCCESS) {
	    Print(L"failed to load bzImage %lx\n", err);
		goto out;
	}

	err = handle_protocol(bz_hd, &LoadedImageProtocol, (void **)&bz_info);
	if (err != EFI_SUCCESS)
		goto out;

	if (cmdline) {
		bz_info->LoadOptions = cmdline;
		bz_info->LoadOptionsSize = (StrLen(cmdline) + 1) * sizeof(CHAR16);
	}

	pe_entry = get_pe_entry(bz_info->ImageBase);

	if (pe_entry == NULL) {
		Print(L"fail to get pe entry of bzImage\n");
		goto out;
	}

	err = emalloc(sizeof(struct efi_ctx), 8, &addr);
	if (err != EFI_SUCCESS)
		goto out;
	pe = (struct efi_ctx*)(UINTN)addr;
	pe->entry = pe_entry;
	pe->handle = bz_hd;
	pe->table = sys_table;


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


	EFI_CONFIGURATION_TABLE *config_table = sys_table->ConfigurationTable;
	for (i = 0; i < sys_table->NumberOfTableEntries;i++) {
		EFI_GUID acpi_20_table_guid = ACPI_20_TABLE_GUID;
		EFI_GUID acpi_table_guid = ACPI_TABLE_GUID;
		if (CompareGuid(&acpi_20_table_guid, &config_table->VendorGuid) == 0) {
			rsdp = config_table->VendorTable;
			break;
		}

		if (CompareGuid(&acpi_table_guid, &config_table->VendorGuid) == 0)
			rsdp = config_table->VendorTable;

		config_table++;
	}
	
	if (!rsdp) {
		Print(L"unable to find RSDP\n");
		goto out;
	}


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
		if (e820_type == E820_RAM) {
			UINT64 start = d->PhysicalStart;
			UINT64 end  =  d->PhysicalStart + (d->NumberOfPages<<EFI_PAGE_SHIFT);
			if (start <= ACRN_HV_ADDR && end > (ACRN_HV_ADDR  + ACRN_HV_SIZE))
				Print(L"e820[%d] start=%lx len=%lx\n", i, d->PhysicalStart, d->NumberOfPages << EFI_PAGE_SHIFT);
		}

		if (j && mmap[j-1].mm_type == e820_type &&
			(mmap[j-1].mm_base_addr + mmap[j-1].mm_length) == d->PhysicalStart) {
			mmap[j-1].mm_length += d->NumberOfPages << EFI_PAGE_SHIFT;
		} else {
			mmap[j].mm_base_addr = d->PhysicalStart;
			mmap[j].mm_length = d->NumberOfPages << EFI_PAGE_SHIFT;
			mmap[j].mm_type = e820_type;
			j++;
		}
	}

	/* switch hv memory region(0x20000000 ~ 0x22000000) to availiable RAM in e820 table */
	mmap[j].mm_base_addr = ACRN_HV_ADDR;
	mmap[j].mm_length = ACRN_HV_SIZE;
	mmap[j].mm_type = E820_RAM;
	j++;

	/* reserve secondary memory region(0x1000 ~ 0x10000) for hv */
	err = __emalloc(ACRN_SECONDARY_SIZE, ACRN_SECONDARY_ADDR, &addr, EfiReservedMemoryType);
	if (err != EFI_SUCCESS)
		goto out;

	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP | MULTIBOOT_INFO_HAS_CMDLINE;
	mbi->mi_mmap_length = j*sizeof(struct multiboot_mmap);

	//mbi->mi_cmdline = (UINTN)"uart=mmio@0x92230000";
	//mbi->mi_cmdline = (UINTN)"uart=port@0x3F8";
	mbi->mi_cmdline = (UINTN)"uart=disabled";
	mbi->mi_mmap_addr = (UINTN)mmap;

	pe->rsdp = rsdp;

	//Print(L"start 9!\n");

	asm volatile ("mov %%cr0, %0":"=r"(pe->cr0));
	asm volatile ("mov %%cr3, %0":"=r"(pe->cr3));
	asm volatile ("mov %%cr4, %0":"=r"(pe->cr4));
	asm volatile ("sidt %0" :: "m" (pe->idt));
	asm volatile ("sgdt %0" :: "m" (pe->gdt));
	asm volatile ("str %0" :: "m" (pe->tr_sel));
	asm volatile ("sldt %0" :: "m" (pe->ldt_sel));

	asm volatile ("mov %%cs, %%ax": "=a"(pe->cs_sel));
	asm volatile ("lar %%eax, %%eax"
					:"=a"(pe->cs_ar)
					:"a"(pe->cs_sel)
					);
	pe->cs_ar = (pe->cs_ar >> 8) & 0xf0ff; /* clear bits 11:8 */

	asm volatile ("mov %%es, %%ax": "=a"(pe->es_sel));
	asm volatile ("mov %%ss, %%ax": "=a"(pe->ss_sel));
	asm volatile ("mov %%ds, %%ax": "=a"(pe->ds_sel));
	asm volatile ("mov %%fs, %%ax": "=a"(pe->fs_sel));
	asm volatile ("mov %%gs, %%ax": "=a"(pe->gs_sel));


	uint32_t idx = 0xC0000080; /* MSR_IA32_EFER */
	uint32_t msrl, msrh;
	asm volatile ("rdmsr":"=a"(msrl), "=d"(msrh): "c"(idx));
	pe->efer = ((uint64_t)msrh<<32) | msrl;

	asm volatile ("pushf\n\t"
					"pop %0\n\t"
					:"=r"(pe->rflags):);

	asm volatile ("movq %%rsp, %0":"=r"(pe->rsp));

	hv_jump(ACRN_HV_ADDR, mbi, pe);
out:
	return err;
}


static EFI_STATUS
parse_args(CHAR16 *options, UINT32 size, CHAR16 **name,
		CHAR16 **hcmdline, CHAR16 **scmdline)
{
	CHAR16 *n, *p, *cmdline, *search;
	UINTN i = 0;

	*hcmdline = NULL;
	*scmdline = NULL;
	*name = NULL;

	cmdline = StrDuplicate(options);

	search = PoolPrint(L"sos=");
	n = strstr_16(cmdline, search);
	if (!n) {
		Print(L"Failed to get sos\n");
		return EFI_OUT_OF_RESOURCES;
	}
	FreePool(search);


	n += 4;
	p = n;
	i = 0;
	while (*n && !isspace((CHAR8)*n)) {
		n++; i++;
	}
	*n++ = '\0';
	*name = p;

	*scmdline = n;

	return EFI_SUCCESS;
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
	CHAR16 *options = NULL, *name;
	UINT32 options_size = 0;
	CHAR16 *hcmdline, *scmdline;
	UINTN sec_addr;
	UINTN sec_size;
	char *section;


	InitializeLib(image, _table);

	sys_table = _table;
	boot = sys_table->BootServices;
	runtime = sys_table->RuntimeServices;

	if (CheckCrc(sys_table->Hdr.HeaderSize, &sys_table->Hdr) != TRUE)
		return EFI_LOAD_ERROR;


	err = handle_protocol(image, &LoadedImageProtocol, (void **)&info);
	if (err != EFI_SUCCESS)
		goto failed;

	options = info->LoadOptions;
	options_size = info->LoadOptionsSize;

	err = parse_args(options,  options_size, &name, &hcmdline, &scmdline);
	if (err != EFI_SUCCESS) 
		return err;

	section = ".hv";
	err = get_pe_section(info->ImageBase, section, &sec_addr, &sec_size);
	if (EFI_ERROR(err)) {
		Print(L"Unable to locate section of ACRNHV %r ", err);
		goto failed;
	}

  	err = __emalloc(ACRN_HV_SIZE, ACRN_HV_ADDR, &addr, EfiReservedMemoryType);
	if (err != EFI_SUCCESS)
		goto failed;

	/* Copy ACRNHV binary to fixed phys addr. LoadImage and StartImage ?? */
	memcpy((char*)addr, info->ImageBase + sec_addr, sec_size);

	/* load sos and run hypervisor */
	err = load_sos_image(image, name, scmdline);

	if (err != EFI_SUCCESS)
		goto free_args;

	return EFI_SUCCESS;

free_args:
	free(name);
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

