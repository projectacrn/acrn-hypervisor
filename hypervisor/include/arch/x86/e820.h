/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

struct e820_mem_params {
	uint64_t mem_bottom;
	uint64_t mem_top;
	uint64_t total_mem_size;
	uint64_t max_ram_blk_base; /* used for the start address of UOS */
	uint64_t max_ram_blk_size;
};

void init_e820(void);
void obtain_e820_mem_info(void);
void rebuild_vm0_e820(void);
uint64_t e820_alloc_low_memory(uint32_t size_arg);

extern uint32_t e820_entries;
extern struct e820_entry e820[E820_MAX_ENTRIES];
extern struct e820_mem_params e820_mem;

uint32_t create_e820_table(struct e820_entry *param_e820);

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
