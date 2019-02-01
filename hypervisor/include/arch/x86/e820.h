/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef E820_H
#define E820_H

/* E820 memory types */
#define E820_TYPE_RAM		1U	/* EFI 1, 2, 3, 4, 5, 6, 7 */
#define E820_TYPE_RESERVED	2U
/* EFI 0, 11, 12, 13 (everything not used elsewhere) */
#define E820_TYPE_ACPI_RECLAIM	3U	/* EFI 9 */
#define E820_TYPE_ACPI_NVS	4U	/* EFI 10 */
#define E820_TYPE_UNUSABLE	5U	/* EFI 8 */

/** Defines a single entry in an E820 memory map. */
struct e820_entry {
   /** The base address of the memory range. */
	uint64_t baseaddr;
   /** The length of the memory range. */
	uint64_t length;
   /** The type of memory region. */
	uint32_t type;
} __packed;

struct e820_mem_params {
	uint64_t mem_bottom;
	uint64_t mem_top;
	uint64_t total_mem_size;
	uint64_t max_ram_blk_base; /* used for the start address of UOS */
	uint64_t max_ram_blk_size;
};

/* HV read multiboot header to get e820 entries info and calc total RAM info */
void init_e820(void);

/* get some RAM below 1MB in e820 entries, hide it from sos_vm, return its start address */
uint64_t e820_alloc_low_memory(uint32_t size_arg);

/* get total number of the e820 entries */
uint32_t get_e820_entries_count(void);

/* get the e802 entiries */
const struct e820_entry *get_e820_entry(void);

/* get the e820 total memory info */
const struct e820_mem_params *get_e820_mem_info(void);

#endif
