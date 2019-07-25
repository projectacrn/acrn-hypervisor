/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board.h>
#include <msr.h>

struct platform_clos_info platform_clos_array[4] = {
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_0,
	},
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_1,
	},
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_2,
	},
	{
		.clos_mask = 0xff,
		.msr_index = MSR_IA32_L2_MASK_3,
	},
};

uint16_t platform_clos_num = (uint16_t)(sizeof(platform_clos_array)/sizeof(struct platform_clos_info));

const struct cpu_state_table board_cpu_state_tbl;
