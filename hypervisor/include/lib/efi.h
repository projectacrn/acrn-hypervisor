/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EFI_H
#define EFI_H

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
