/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RTCT_H
#define RTCT_H

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
	uint32_t rtct_ver_major;
	uint32_t rtct_ver_minor;
	uint32_t rtcd_ver_major;
	uint32_t rtcd_ver_Minor;
} __packed;

struct rtct_entry_data_ssram_waymask {
	uint32_t cache_level;
	uint32_t cache_id;
	uint32_t waymask;
} __packed;

struct rtct_entry_data_ssram_v2 {
	uint32_t cache_level;
	uint32_t cache_id;
	uint64_t base;
	uint32_t size;
	uint32_t shared;
} __packed;

uint64_t get_vssram_gpa_base(void);
uint64_t get_vssram_size(void);
uint8_t *get_vssram_vrtct(void);
void clean_vssram_configs(void);
int init_vssram(struct vmctx *ctx);
void deinit_vssram(struct vmctx *ctx);
int parse_vssram_buf_params(const char *opt);

#endif  /* RTCT_H */
