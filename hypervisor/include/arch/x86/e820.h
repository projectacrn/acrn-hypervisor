/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef E820_H
#define E820_H

struct e820_mem_params {
	uint64_t mem_bottom;
	uint64_t mem_top;
	uint64_t total_mem_size;
	uint64_t max_ram_blk_base; /* used for the start address of UOS */
	uint64_t max_ram_blk_size;
};

/* HV read multiboot header to get e820 entries info and calc total RAM info */
void init_e820(void);

/* before boot sos_vm(service OS), call it to hide the HV RAM entry in e820 table from sos_vm */
void rebuild_sos_vm_e820(void);

/* get some RAM below 1MB in e820 entries, hide it from sos_vm, return its start address */
uint64_t e820_alloc_low_memory(uint32_t size_arg);

/* copy the original e820 entries info to param_e820 */
uint32_t create_e820_table(struct e820_entry *param_e820);

/* get total number of the e820 entries */
uint32_t get_e820_entries_count(void);

/* get the e802 entiries */
const struct e820_entry *get_e820_entry(void);

/* get the e820 total memory info */
const struct e820_mem_params *get_e820_mem_info(void);

#ifdef CONFIG_PARTITION_MODE
/*
 * Default e820 mem map:
 *
 * Assumption is every VM launched by ACRN in partition mode uses 2G of RAM.
 * there is reserved memory of 64K for MPtable and PCI hole of 512MB
 */
#define NUM_E820_ENTRIES        5U
extern const struct e820_entry e820_default_entries[NUM_E820_ENTRIES];
#endif

#endif
