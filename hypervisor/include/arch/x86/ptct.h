/*
 * Copyright (C) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTCT_H
#define PTCT_H

#include <acpi.h>


#define PTCT_ENTRY_TYPE_PTCD_LIMIT		1U
#define PTCT_ENTRY_TYPE_PTCM_BINARY		2U
#define PTCT_ENTRY_TYPE_WRC_L3_MASKS		3U
#define PTCT_ENTRY_TYPE_GT_L3_MASKS		4U
#define PTCT_ENTRY_TYPE_PSRAM			5U
#define PTCT_ENTRY_TYPE_STREAM_DATAPATH		6U
#define PTCT_ENTRY_TYPE_TIMEAWARE_SUBSYS	7U
#define PTCT_ENTRY_TYPE_RT_IOMMU		8U
#define PTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY	9U

#define PSRAM_BASE_HPA 0x40080000U
#define PSRAM_BASE_GPA 0x40080000U
#define PSRAM_MAX_SIZE 0x00800000U

struct ptct_entry{
	 uint16_t size;
	 uint16_t format;
	 uint32_t type;
	 uint32_t data[64];
} __packed;

struct ptct_entry_data_ptcm_binary
{
	uint64_t address;
	uint32_t size;
} __packed;

struct ptct_entry_data_psram
{
	uint32_t cache_level;
	uint64_t base;
	uint32_t ways;
	uint32_t size;
	uint32_t apic_id_0; /*only the first core is responsible for initialization of L3 mem region*/
} __packed;


extern uint64_t psram_area_bottom;
extern uint64_t psram_area_top;

#endif /* PTCT_H */
