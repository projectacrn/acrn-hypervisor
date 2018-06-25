/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/* The table includes cpu px info of Intel A3960 SoC */
static const struct cpu_px_data px_a3960[] = {
	{0x960, 0, 0xA, 0xA, 0x1800, 0x1800}, /* P0 */
	{0x8FC, 0, 0xA, 0xA, 0x1700, 0x1700}, /* P1 */
	{0x898, 0, 0xA, 0xA, 0x1600, 0x1600}, /* P2 */
	{0x834, 0, 0xA, 0xA, 0x1500, 0x1500}, /* P3 */
	{0x7D0, 0, 0xA, 0xA, 0x1400, 0x1400}, /* P4 */
	{0x76C, 0, 0xA, 0xA, 0x1300, 0x1300}, /* P5 */
	{0x708, 0, 0xA, 0xA, 0x1200, 0x1200}, /* P6 */
	{0x6A4, 0, 0xA, 0xA, 0x1100, 0x1100}, /* P7 */
	{0x640, 0, 0xA, 0xA, 0x1000, 0x1000}, /* P8 */
	{0x5DC, 0, 0xA, 0xA, 0x0F00, 0x0F00}, /* P9 */
	{0x578, 0, 0xA, 0xA, 0x0E00, 0x0E00}, /* P10 */
	{0x514, 0, 0xA, 0xA, 0x0D00, 0x0D00}, /* P11 */
	{0x4B0, 0, 0xA, 0xA, 0x0C00, 0x0C00}, /* P12 */
	{0x44C, 0, 0xA, 0xA, 0x0B00, 0x0B00}, /* P13 */
	{0x3E8, 0, 0xA, 0xA, 0x0A00, 0x0A00}, /* P14 */
	{0x384, 0, 0xA, 0xA, 0x0900, 0x0900}, /* P15 */
	{0x320, 0, 0xA, 0xA, 0x0800, 0x0800}  /* P16 */
};

/* The table includes cpu cx info of Intel A3960 SoC */
static const struct cpu_cx_data cx_a3960[] = {
	{{SPACE_FFixedHW,  0x0, 0, 0,     0}, 0x1, 0x1, 0x3E8}, /* C1 */
	{{SPACE_SYSTEM_IO, 0x8, 0, 0, 0x415}, 0x2, 0x32, 0x0A}, /* C2 */
	{{SPACE_SYSTEM_IO, 0x8, 0, 0, 0x419}, 0x3, 0x96, 0x0A}  /* C3 */
};

/* The table includes cpu px info of Intel A3950 SoC */
static const struct cpu_px_data px_a3950[] = {
	{0x7D0, 0, 0xA, 0xA, 0x1400, 0x1400}, /* P0 */
	{0x76C, 0, 0xA, 0xA, 0x1300, 0x1300}, /* P1 */
	{0x708, 0, 0xA, 0xA, 0x1200, 0x1200}, /* P2 */
	{0x6A4, 0, 0xA, 0xA, 0x1100, 0x1100}, /* P3 */
	{0x640, 0, 0xA, 0xA, 0x1000, 0x1000}, /* P4 */
	{0x5DC, 0, 0xA, 0xA, 0x0F00, 0x0F00}, /* P5 */
	{0x578, 0, 0xA, 0xA, 0x0E00, 0x0E00}, /* P6 */
	{0x514, 0, 0xA, 0xA, 0x0D00, 0x0D00}, /* P7 */
	{0x4B0, 0, 0xA, 0xA, 0x0C00, 0x0C00}, /* P8 */
	{0x44C, 0, 0xA, 0xA, 0x0B00, 0x0B00}, /* P9 */
	{0x3E8, 0, 0xA, 0xA, 0x0A00, 0x0A00}, /* P10 */
	{0x384, 0, 0xA, 0xA, 0x0900, 0x0900}, /* P11 */
	{0x320, 0, 0xA, 0xA, 0x0800, 0x0800}  /* P12 */
};

/* The table includes cpu px info of Intel J3455 SoC */
static const struct cpu_px_data px_j3455[] = {
	{0x5DD, 0, 0xA, 0xA, 0x1700, 0x1700}, /* P0 */
	{0x5DC, 0, 0xA, 0xA, 0x0F00, 0x0F00}, /* P1 */
	{0x578, 0, 0xA, 0xA, 0x0E00, 0x0E00}, /* P2 */
	{0x514, 0, 0xA, 0xA, 0x0D00, 0x0D00}, /* P3 */
	{0x4B0, 0, 0xA, 0xA, 0x0C00, 0x0C00}, /* P4 */
	{0x44C, 0, 0xA, 0xA, 0x0B00, 0x0B00}, /* P5 */
	{0x3E8, 0, 0xA, 0xA, 0x0A00, 0x0A00}, /* P6 */
	{0x384, 0, 0xA, 0xA, 0x0900, 0x0900}, /* P7 */
	{0x320, 0, 0xA, 0xA, 0x0800, 0x0800}  /* P8 */
};

/* The table includes cpu cx info of Intel J3455 SoC */
static const struct cpu_cx_data cx_j3455[] = {
	{{SPACE_FFixedHW, 0x1, 0x2, 0x1, 0x01}, 0x1, 0x1, 0x3E8}, /* C1 */
	{{SPACE_FFixedHW, 0x1, 0x2, 0x1, 0x21}, 0x2, 0x32, 0x0A}, /* C2 */
	{{SPACE_FFixedHW, 0x1, 0x2, 0x1, 0x60}, 0x3, 0x96, 0x0A}  /* C3 */
};

static const struct cpu_state_table {
	char			model_name[64];
	struct cpu_state_info	state_info;
} cpu_state_tbl[] = {
	{"Intel(R) Atom(TM) Processor A3960 @ 1.90GHz",
		{ARRAY_SIZE(px_a3960), px_a3960,
		 ARRAY_SIZE(cx_a3960), cx_a3960}
	},
	{"Intel(R) Atom(TM) Processor A3950 @ 1.60GHz",
		{ARRAY_SIZE(px_a3950), px_a3950,
		 ARRAY_SIZE(cx_a3960), cx_a3960} /* Cx is same as A3960 */
	},
	{"Intel(R) Celeron(R) CPU J3455 @ 1.50GHz",
		{ARRAY_SIZE(px_j3455), px_j3455,
		 ARRAY_SIZE(cx_j3455), cx_j3455}
	}
};

static int get_state_tbl_idx(char *cpuname)
{
	int i;
	int count = ARRAY_SIZE(cpu_state_tbl);

	if (cpuname == NULL) {
		return -1;
	}

	for (i = 0; i < count; i++) {
		if (strcmp((cpu_state_tbl[i].model_name),
				cpuname) == 0) {
			return i;
		}
	}

	return -1;
}

void load_cpu_state_data(void)
{
	int tbl_idx;
	const struct cpu_state_info *state_info;

	memset(&boot_cpu_data.state_info, 0,
			sizeof(struct cpu_state_info));

	tbl_idx = get_state_tbl_idx(boot_cpu_data.model_name);
	if (tbl_idx < 0) {
		/* The state table is not found. */
		return;
	}

	state_info = &(cpu_state_tbl + tbl_idx)->state_info;

	if ((state_info->px_cnt != 0U) && (state_info->px_data != NULL)) {
		if (state_info->px_cnt > MAX_PSTATE) {
			boot_cpu_data.state_info.px_cnt = MAX_PSTATE;
		} else {
			boot_cpu_data.state_info.px_cnt = state_info->px_cnt;
		}

		boot_cpu_data.state_info.px_data = state_info->px_data;
	}

	if ((state_info->cx_cnt != 0U) && (state_info->cx_data != NULL)) {
		if (state_info->cx_cnt > MAX_CX_ENTRY) {
			boot_cpu_data.state_info.cx_cnt = MAX_CX_ENTRY;
		} else {
			boot_cpu_data.state_info.cx_cnt = state_info->cx_cnt;
		}

		boot_cpu_data.state_info.cx_data = state_info->cx_data;
	}

}
