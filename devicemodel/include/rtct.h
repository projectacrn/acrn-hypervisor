/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RTCT_H
#define RTCT_H

#define RTCT_ENTRY_TYPE_PTCD_LIMIT		1U
#define RTCT_ENTRY_TYPE_PTCM_BINARY		2U
#define RTCT_ENTRY_TYPE_WRC_L3_MASKS		3U
#define RTCT_ENTRY_TYPE_GT_L3_MASKS		4U
#define RTCT_ENTRY_TYPE_SSRAM			5U
#define RTCT_ENTRY_TYPE_STREAM_DATAPATH		6U
#define RTCT_ENTRY_TYPE_TIMEAWARE_SUBSYS	7U
#define RTCT_ENTRY_TYPE_RT_IOMMU		8U
#define RTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY	9U

/*Entry IDs for RTCT version 2*/
#define	RTCT_V2_COMPATIBILITY	0U
#define	RTCT_V2_RTCD_LIMIT	1U
#define	RTCT_V2_CRL_BINARY	2U
#define	RTCT_V2_IA_WAYMASK	3U
#define	RTCT_V2_WRC_WAYMASK	4U
#define	RTCT_V2_GT_WAYMASK	5U
#define	RTCT_V2_SSRAM_WAYMASK	6U
#define	RTCT_V2_SSRAM	7U
#define	RTCT_V2_MEMORY_HIERARCHY_LATENCY	8U
#define	RTCT_V2_ERROR_LOG_ADDRESS	9U

struct rtct_entry {
	uint16_t size;
	uint16_t format_version;
	uint32_t type;
	uint32_t data[64];
} __packed;

struct rtct_entry_data_compatibility {
	uint32_t RTCT_Ver_Major;
	uint32_t RTCT_Ver_Minor;
	uint32_t RTCD_Ver_Major;
	uint32_t RTCD_Ver_Minor;
} __packed;

struct rtct_entry_data_ssram {
	uint32_t cache_level;
	uint64_t base;
	uint32_t ways;
	uint32_t size;
	uint32_t apic_id_tbl[64];
} __packed;

struct rtct_entry_data_ssram_v2 {
	uint32_t cache_level;
	uint32_t cache_id;
	uint64_t base;
	uint32_t size;
	uint32_t shared;
} __packed;

struct rtct_entry_data_mem_hi_latency {
	uint32_t hierarchy;
	uint32_t clock_cycles;
	uint32_t apic_id_tbl[64];
} __packed;

uint64_t get_software_sram_base_hpa(void);
uint64_t get_software_sram_size(void);
uint8_t *build_vrtct(struct vmctx *ctx, void *cfg);

#endif  /* RTCT_H */
