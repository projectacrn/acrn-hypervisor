/*
 * Copyright (C) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RTCM_H
#define RTCM_H

#include <asm/rtct.h>

#define MSABI __attribute__((ms_abi))

typedef int32_t MSABI(*rtcm_abi_func)(uint32_t command, void *command_struct);

#define RTCM_CMD_INIT_SOFTWARE_SRAM	(int32_t)1U
#define RTCM_CMD_CPUID			(int32_t)2U
#define RTCM_CMD_RDMSR			(int32_t)3U
#define RTCM_CMD_WRMSR			(int32_t)4U

#define RTCM_MAGIC_PTCM 0x5054434dU /* "PTCM", CRL header magic number for RTCT version 1 */
#define RTCM_MAGIC_RTCM 0x5254434dU /* "RTCM", CRL header magic number for RTCT version 2 */

struct rtcm_header {
	uint32_t magic;
	uint32_t version;
	uint64_t command_offset;
} __packed;

bool init_software_sram(bool is_bsp);
void set_rtct_tbl(void *rtct_tbl_addr);
bool is_software_sram_enabled(void);
#endif /* RTCM_H */
