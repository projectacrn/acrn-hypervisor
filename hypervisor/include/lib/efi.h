/*
 * Copyright (C) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EFI_H
#define EFI_H

/* EFI Memory Attribute values: */
#define EFI_MEMORY_UC			0x0000000000000001UL	/* uncached */
#define EFI_MEMORY_WC			0x0000000000000002UL	/* write-coalescing */
#define EFI_MEMORY_WT			0x0000000000000004UL	/* write-through */
#define EFI_MEMORY_WB			0x0000000000000008UL	/* write-back */
/*
#define EFI_MEMORY_UCE			0x0000000000000010UL
#define EFI_MEMORY_WP			0x0000000000001000UL
#define EFI_MEMORY_RP			0x0000000000002000UL
#define EFI_MEMORY_XP			0x0000000000004000UL
#define EFI_MEMORY_NV			0x0000000000008000UL
#define EFI_MEMORY_MORE_RELIABLE 	0x0000000000010000UL
#define EFI_MEMORY_RO			0x0000000000020000UL
#define EFI_MEMORY_SP			0x0000000000040000UL
#define EFI_MEMORY_CPU_CRYPTO		0x0000000000080000UL
#define EFI_MEMORY_RUNTIME		0x8000000000000000UL
*/

enum efi_memory_type {
	EFI_RESERVED_MEMORYTYPE,
	EFI_LOADER_CODE,
	EFI_LOADER_DATA,
	EFI_BOOT_SERVICES_CODE,
	EFI_BOOT_SERVICES_DATA,
	EFI_RUNTIME_SERVICES_CODE,
	EFI_RUNTIME_SERVICES_DATA,
	EFI_CONVENTIONAL_MEMORY,
	EFI_UNUSABLE_MEMORY,
	EFI_ACPI_RECLAIM_MEMORY,
	EFI_ACPI_MEMORY_NVS,
	EFI_MEMORYMAPPED_IO,
	EFI_MEMORYMAPPED_IOPORTSPACE,
	EFI_PALCODE,
	EFI_PERSISTENT_MEMORY,
	EFI_MAX_MEMORYTYPE
};

struct efi_memory_desc {
	uint32_t type;
	uint32_t pad;
	uint64_t phys_addr;
	uint64_t virt_addr;
	uint64_t num_pages;
	uint64_t attribute;
};

struct efi_info {
	uint32_t loader_signature;	/* 0x1c0 */
	uint32_t systab;		/* 0x1c4 */
	uint32_t memdesc_size;		/* 0x1c8 */
	uint32_t memdesc_version;	/* 0x1cc */
	uint32_t memmap;		/* 0x1d0 */
	uint32_t memmap_size;		/* 0x1d4 */
	uint32_t systab_hi;		/* 0x1d8 */
	uint32_t memmap_hi;		/* 0x1dc */
} __packed;

#endif
