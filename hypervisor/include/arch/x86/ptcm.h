/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTCM_H
#define PTCM_H

#include <ptct.h>

#define MSABI __attribute__((ms_abi))

typedef int32_t MSABI (*ptcm_command_abi)(uint32_t command, void *command_struct);

#define PTCM_CMD_INIT_PSRAM (int32_t)1U
#define PTCM_CMD_CPUID (int32_t)2U
#define PTCM_CMD_RDMSR (int32_t)3U
#define PTCM_CMD_WRMSR (int32_t)4U

#define PCTM_L2_CLOS_MASK_MAX_NUM 8U
#define PCTM_L3_CLOS_MASK_MAX_NUM 4U

#define PTCM_STATUS_SUCCESS 0
#define PTCM_STATUS_FAILURE -1

struct ptcm_info
{
    uint32_t version;           // [OUT]
    uint32_t max_command_index; // [OUT]
};

struct ptcm_header
{
    uint32_t magic;
    uint32_t version;
    uint64_t command_interface_offset;
};

#endif /* PTCM_H */
