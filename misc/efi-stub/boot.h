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

#ifndef __ACRNBOOT_H__
#define __ACRNBOOT_H__

#include "multiboot.h"

#define E820_RAM		1
#define E820_RESERVED		2
#define E820_ACPI		3
#define E820_NVS		4
#define E820_UNUSABLE		5

#define ERROR_STRING_LENGTH	32
#define EFI_LOADER_SIGNATURE    "EL64"

#define ACPI_XSDT_ENTRY_SIZE     (sizeof(UINT64))
#define ACPI_NAME_SIZE                  4
#define ACPI_OEM_ID_SIZE                6
#define ACPI_OEM_TABLE_ID_SIZE          8

#define MSR_IA32_PAT                        0x00000277  /* PAT */
#define MSR_IA32_EFER                       0xC0000080
#define MSR_IA32_FS_BASE                    0xC0000100U
#define MSR_IA32_GS_BASE                    0xC0000101
#define MSR_IA32_SYSENTER_ESP               0x00000175  /* ESP for sysenter */
#define MSR_IA32_SYSENTER_EIP               0x00000176  /* EIP for sysenter */

#define UEFI_BOOT_LOADER_NAME "ACRN UEFI loader"

#define EFI_VAR_MORCTL_NAME                    L"MemoryOverwriteRequestControl"
#define EFI_VAR_MORCTL_GUID \
	   {  0xe20939be, 0x32d4, 0x41be, { 0xa1, 0x50, 0x89, 0x7f, 0x85, 0xd4, 0x98, 0x29 } }
#define EFI_VAR_MORCTLLOCK_NAME                L"MemoryOverwriteRequestControlLock"
#define EFI_VAR_MORCTLLOCK_GUID \
	   {  0xBB983CCF, 0x151D, 0x40E1, { 0xA0, 0x7B, 0x4A, 0x17, 0xBE, 0x16, 0x82, 0x92 } }

#define ALIGN_UP(addr, align) \
	(((addr) + (typeof (addr)) (align) - 1) & ~((typeof (addr)) (align) - 1))

/* Read MSR */
#define CPU_MSR_READ(reg, msr_val_ptr)                      \
{                                                           \
	uint32_t msrl, msrh;                                 \
	asm volatile ("rdmsr" : "=a"(msrl),                 \
		"=d"(msrh) : "c" (reg));            \
	*msr_val_ptr = ((uint64_t)msrh << 32U) | msrl;           \
}

EFI_STATUS get_pe_section(CHAR8 *base, char *section_name, UINTN section_name_len, UINTN *vaddr, UINTN *size);
typedef void(*hv_func)(int32_t, struct multiboot_info*);

/*
 * We allocate memory for the following struct together with hyperivosr itself
 * memory allocation during boot.
 */
#define MBOOT_MMAP_NUMS        256
#define MBOOT_MMAP_SIZE (sizeof(struct multiboot_mmap) * MBOOT_MMAP_NUMS)
#define MBOOT_INFO_SIZE (sizeof(struct multiboot_info))
#define MBOOT_MODS_NUMS        4
#define MBOOT_MODS_SIZE (sizeof(struct multiboot_module) * MBOOT_MODS_NUMS)
#define BOOT_LOADER_NAME_SIZE 17U
#define EFI_BOOT_MEM_SIZE \
	(MBOOT_MMAP_SIZE + MBOOT_INFO_SIZE + MBOOT_MODS_SIZE + BOOT_LOADER_NAME_SIZE)
#define MBOOT_MMAP_PTR(addr) \
	((struct multiboot_mmap *)((VOID *)(addr)))
#define MBOOT_INFO_PTR(addr)  \
	((struct multiboot_info *)((VOID *)(addr) + MBOOT_MMAP_SIZE))
#define MBOOT_MODS_PTR(addr)  \
	((struct multiboot_module *)((VOID *)(addr) + MBOOT_MMAP_SIZE + MBOOT_INFO_SIZE))
#define BOOT_LOADER_NAME_PTR(addr)	\
	((char *)((VOID *)(addr) + MBOOT_MMAP_SIZE + MBOOT_INFO_SIZE + MBOOT_MODS_SIZE))

struct efi_memmap_info {
	UINTN map_size;
	UINTN map_key;
	UINT32 desc_version;
	UINTN desc_size;
	EFI_MEMORY_DESCRIPTOR *mmap;
};

typedef struct mb_module_info {
	UINTN mod_start;
	UINTN mod_end;
	const char *cmd;
	UINTN cmdsize;
} MB_MODULE_INFO;

struct efi_info {
	UINT32 efi_loader_signature;
	UINT32 efi_systab;
	UINT32 efi_memdesc_size;
	UINT32 efi_memdesc_version;
	UINT32 efi_memmap;
	UINT32 efi_memmap_size;
	UINT32 efi_systab_hi;
	UINT32 efi_memmap_hi;
};

struct e820_entry {
	UINT64 addr;		/* start of memory segment */
	UINT64 size;		/* size of memory segment */
	UINT32 type;		/* type of memory segment */
} __attribute__((packed));

struct acpi_table_rsdp {
	/* ACPI signature, contains "RSD PTR " */
	char signature[8];
	/* ACPI 1.0 checksum */
	UINT8 checksum;
	/* OEM identification */
	char oem_id[ACPI_OEM_ID_SIZE];
	/* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	UINT8 revision;
	/* 32-bit physical address of the RSDT */
	UINT32 rsdt_physical_address;
	/* Table length in bytes, including header (ACPI 2.0+) */
	UINT32 length;
	/* 64-bit physical address of the XSDT (ACPI 2.0+) */
	UINT64 xsdt_physical_address;
	/* Checksum of entire table (ACPI 2.0+) */
	UINT8 extended_checksum;
	/* Reserved, must be zero */
	UINT8 reserved[3];
};

struct acpi_table_header {
	 /* ASCII table signature */
	char signature[ACPI_NAME_SIZE];
	/* Length of table in bytes, including this header */
	UINT32 length;
	/* ACPI Specification minor version number */
	UINT8 revision;
	/* To make sum of entire table == 0 */
	UINT8 checksum;
	/* ASCII OEM identification */
	char oem_id[ACPI_OEM_ID_SIZE];
	/* ASCII OEM table identification */
	char oem_table_id[ACPI_OEM_TABLE_ID_SIZE];
	/* OEM revision number */
	UINT32 oem_revision;
	/* ASCII ASL compiler vendor ID */
	char asl_compiler_id[ACPI_NAME_SIZE];
	/* ASL compiler version */
	UINT32 asl_compiler_revision;
};

/* hypervisor loader operation table */
typedef struct hv_loader *HV_LOADER;
struct hv_loader {
	/* Load ACRN hypervisor image into memory */
	EFI_STATUS (*load_boot_image)(IN HV_LOADER hvld);
	/* Load VM Kernels and ACPI Tables into memory */
	EFI_STATUS (*load_modules)(IN HV_LOADER hvld);

	const char *(*get_boot_cmd)(IN HV_LOADER hvld);
	/* Get hypervisor boot command length */
	UINTN (*get_boot_cmdsize)(IN HV_LOADER hvld);

	MB_MODULE_INFO *(*get_mods_info)(IN HV_LOADER hvld, UINTN index);
	/* Get the number of multiboot2 modules */
	UINTN (*get_mod_count)(IN HV_LOADER hvld);
	/* Get the total memory size allocated to load module files */
	UINTN (*get_total_modsize)(IN HV_LOADER hvld);
	/* Get the total lengths of the module commands */
	UINTN (*get_total_modcmdsize)(IN HV_LOADER hvld);

	/* Get the total memory size of hv image */
	UINTN (*get_hv_ram_size)(IN HV_LOADER hvld);

	/* Get the start address of the memory region stored ACRN hypervisor image */
	EFI_PHYSICAL_ADDRESS (*get_hv_hpa)(IN HV_LOADER hvld);
	/* Get the start address of the memory region stored module files */
	EFI_PHYSICAL_ADDRESS (*get_mod_hpa)(IN HV_LOADER hvld);
	/* Get the entry point of ACRN hypervisor */
	EFI_PHYSICAL_ADDRESS (*get_hv_entry)(IN HV_LOADER hvld);

	/* Get the supported multiboot version of ACRN hypervisor image */
	int (*get_multiboot_version)(IN HV_LOADER hvld);

	/* free up memory allocated by hypervisor loader */
	void (*deinit)(IN HV_LOADER hvld);
};

EFI_STATUS get_efi_memmap(struct efi_memmap_info *mmap_info, int size_only);

static inline uint64_t
msr_read(uint32_t reg_num)
{
	uint64_t msr_val;

	CPU_MSR_READ(reg_num, &msr_val);
	return msr_val;
}

#endif
