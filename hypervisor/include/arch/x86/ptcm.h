/*
 * Copyright (C) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTCM_H
#define PTCM_H

#include <ptct.h>

#define MSABI __attribute__((ms_abi))

typedef int32_t MSABI (*ptcm_abi_func)(uint32_t command, void *command_struct);

#define PTCM_CMD_INIT_PSRAM	(int32_t)1U
#define PTCM_CMD_CPUID		(int32_t)2U
#define PTCM_CMD_RDMSR		(int32_t)3U
#define PTCM_CMD_WRMSR		(int32_t)4U

#define PTCM_MAGIC 0x5054434dU

struct ptcm_header {
	uint32_t magic;
	uint32_t version;
	uint64_t command_offset;
} __packed;

extern volatile bool is_psram_initialized;
void init_psram(bool is_bsp);
void set_ptct_tbl(void *ptct_tbl_addr);
#endif /* PTCM_H */
