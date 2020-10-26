/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTCT_H
#define PTCT_H

#include <acpi.h>

#define MAX_L2_PSRAM_REGIONS 4U
#define MAX_L3_PSRAM_REGIONS 1U

#define PTCT_ENTRY_TYPE_PTCD_LIMIT 1U
#define PTCT_ENTRY_TYPE_PTCM_BINARY 2U
#define PTCT_ENTRY_TYPE_WRC_L3_MASKS 3U
#define PTCT_ENTRY_TYPE_GT_L3_MASKS 4U
#define PTCT_ENTRY_TYPE_PSRAM 5U
#define PTCT_ENTRY_TYPE_STREAM_DATAPATH 6U
#define PTCT_ENTRY_TYPE_TIMEAWARE_SUBSYS 7U
#define PTCT_ENTRY_TYPE_RT_IOMMU 8U
#define PTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY 9U

#define PSRAM_BASE_HPA 0x40080000U
#define PSRAM_BASE_GPA 0x40080000U
#define PSRAM_MAX_SIZE 0x00800000U

struct ptct_entry{
    uint16_t size;
    uint16_t format;
    uint32_t type;
    uint32_t data[64];
} __attribute__((packed));

struct acpi_table_ptct {
    struct acpi_table_header header;
    struct ptct_entry ptct_first_entry;
} __attribute__((packed));

struct ptct_entry_data_ptcm_binary
{
	uint64_t address;
	uint32_t size;
} __attribute__((packed));

struct ptct_entry_data_psram
{
	uint32_t cache_level;
	uint64_t base;
	uint32_t ways;
	uint32_t size;
	uint32_t apic_id_0; /*only the first core is responsible for initialization of L3 mem region*/
} __attribute__((packed));

enum ptcm_cache_level
{
    PTCM_CACHE_LEVEL_EMPTY = 0,
    PTCM_CACHE_LEVEL_2 = 2,
    PTCM_CACHE_LEVEL_3 = 3
};

enum ptcm_clos_index
{
    PTCM_CLOS_INDEX_NOLOCK = 0,
    PTCM_CLOS_INDEX_LOCK,
    PTCM_CLOS_INDEX_SETUP,
    PTCM_CLOS_NUM_OF
};

struct ptcm_mem_region
{
    bool l2_valid;
    bool l3_valid;

    uint64_t l2_base;
    uint64_t l2_size;
    uint32_t l2_ways;
    uint32_t l2_clos[PTCM_CLOS_NUM_OF];

    uint64_t l3_base;
    uint32_t l3_size;
    uint32_t l3_ways;
    uint32_t l3_clos[PTCM_CLOS_NUM_OF];
};

extern uint64_t psram_area_bottom;
extern uint64_t psram_area_top;
#endif	/* PTCT_H */
