/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RTCT_H
#define RTCT_H

#include <acpi.h>

#include "misc_cfg.h"

#define RTCT_ENTRY_TYPE_RTCD_LIMIT		1U
#define RTCT_ENTRY_TYPE_RTCM_BINARY		2U
#define RTCT_ENTRY_TYPE_WRC_L3_MASKS		3U
#define RTCT_ENTRY_TYPE_GT_L3_MASKS		4U
#define RTCT_ENTRY_TYPE_SOFTWARE_SRAM		5U
#define RTCT_ENTRY_TYPE_STREAM_DATAPATH		6U
#define RTCT_ENTRY_TYPE_TIMEAWARE_SUBSYS	7U
#define RTCT_ENTRY_TYPE_RT_IOMMU		8U
#define RTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY	9U

#if !defined(PRE_RTVM_SW_SRAM_BASE_GPA)
/*
 * PRE_RTVM_SW_SRAM_BASE_GPA is for Prelaunch VM only and
 * is configured by config tool on platform that Software SRAM is detected.
 *
 * For cases that Software SRAM is not detected, we still hardcode a dummy
 * placeholder entry in vE820 table of Prelaunch VM to unify the logic
 * to initialize the vE820.
 */
#define PRE_RTVM_SW_SRAM_BASE_GPA 0x40080000U
#endif

#define PRE_RTVM_SW_SRAM_MAX_SIZE  0x00800000U

struct rtct_entry {
	 uint16_t size;
	 uint16_t format;
	 uint32_t type;
	 uint32_t data[64];
} __packed;

struct rtct_entry_data_rtcm_binary
{
	uint64_t address;
	uint32_t size;
} __packed;

struct rtct_entry_data_software_sram
{
	uint32_t cache_level;
	uint64_t base;
	uint32_t ways;
	uint32_t size;
	uint32_t apic_id_0; /*only the first core is responsible for initialization of L3 mem region*/
} __packed;

uint64_t get_software_sram_base(void);
uint64_t get_software_sram_size(void);
#endif /* RTCT_H */
