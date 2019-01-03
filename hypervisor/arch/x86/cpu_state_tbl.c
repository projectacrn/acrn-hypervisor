/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/* The table includes cpu px info of Intel A3960 SoC */
static const struct cpu_px_data px_a3960[] = {
	{0x960UL, 0UL, 0xAUL, 0xAUL, 0x1800UL, 0x1800UL}, /* P0 */
	{0x8FCUL, 0UL, 0xAUL, 0xAUL, 0x1700UL, 0x1700UL}, /* P1 */
	{0x898UL, 0UL, 0xAUL, 0xAUL, 0x1600UL, 0x1600UL}, /* P2 */
	{0x834UL, 0UL, 0xAUL, 0xAUL, 0x1500UL, 0x1500UL}, /* P3 */
	{0x7D0UL, 0UL, 0xAUL, 0xAUL, 0x1400UL, 0x1400UL}, /* P4 */
	{0x76CUL, 0UL, 0xAUL, 0xAUL, 0x1300UL, 0x1300UL}, /* P5 */
	{0x708UL, 0UL, 0xAUL, 0xAUL, 0x1200UL, 0x1200UL}, /* P6 */
	{0x6A4UL, 0UL, 0xAUL, 0xAUL, 0x1100UL, 0x1100UL}, /* P7 */
	{0x640UL, 0UL, 0xAUL, 0xAUL, 0x1000UL, 0x1000UL}, /* P8 */
	{0x5DCUL, 0UL, 0xAUL, 0xAUL, 0x0F00UL, 0x0F00UL}, /* P9 */
	{0x578UL, 0UL, 0xAUL, 0xAUL, 0x0E00UL, 0x0E00UL}, /* P10 */
	{0x514UL, 0UL, 0xAUL, 0xAUL, 0x0D00UL, 0x0D00UL}, /* P11 */
	{0x4B0UL, 0UL, 0xAUL, 0xAUL, 0x0C00UL, 0x0C00UL}, /* P12 */
	{0x44CUL, 0UL, 0xAUL, 0xAUL, 0x0B00UL, 0x0B00UL}, /* P13 */
	{0x3E8UL, 0UL, 0xAUL, 0xAUL, 0x0A00UL, 0x0A00UL}, /* P14 */
	{0x384UL, 0UL, 0xAUL, 0xAUL, 0x0900UL, 0x0900UL}, /* P15 */
	{0x320UL, 0UL, 0xAUL, 0xAUL, 0x0800UL, 0x0800UL}  /* P16 */
};

/* The table includes cpu cx info of Intel A3960 SoC */
static const struct cpu_cx_data cx_a3960[] = {
	{{SPACE_FFixedHW,  0x0U, 0U, 0U,     0UL}, 0x1U, 0x1U, 0x3E8UL}, /* C1 */
	{{SPACE_SYSTEM_IO, 0x8U, 0U, 0U, 0x415UL}, 0x2U, 0x32U, 0x0AUL}, /* C2 */
	{{SPACE_SYSTEM_IO, 0x8U, 0U, 0U, 0x419UL}, 0x3U, 0x96U, 0x0AUL}  /* C3 */
};

/* The table includes cpu px info of Intel A3950 SoC */
static const struct cpu_px_data px_a3950[] = {
	{0x7D0UL, 0UL, 0xAUL, 0xAUL, 0x1400UL, 0x1400UL}, /* P0 */
	{0x76CUL, 0UL, 0xAUL, 0xAUL, 0x1300UL, 0x1300UL}, /* P1 */
	{0x708UL, 0UL, 0xAUL, 0xAUL, 0x1200UL, 0x1200UL}, /* P2 */
	{0x6A4UL, 0UL, 0xAUL, 0xAUL, 0x1100UL, 0x1100UL}, /* P3 */
	{0x640UL, 0UL, 0xAUL, 0xAUL, 0x1000UL, 0x1000UL}, /* P4 */
	{0x5DCUL, 0UL, 0xAUL, 0xAUL, 0x0F00UL, 0x0F00UL}, /* P5 */
	{0x578UL, 0UL, 0xAUL, 0xAUL, 0x0E00UL, 0x0E00UL}, /* P6 */
	{0x514UL, 0UL, 0xAUL, 0xAUL, 0x0D00UL, 0x0D00UL}, /* P7 */
	{0x4B0UL, 0UL, 0xAUL, 0xAUL, 0x0C00UL, 0x0C00UL}, /* P8 */
	{0x44CUL, 0UL, 0xAUL, 0xAUL, 0x0B00UL, 0x0B00UL}, /* P9 */
	{0x3E8UL, 0UL, 0xAUL, 0xAUL, 0x0A00UL, 0x0A00UL}, /* P10 */
	{0x384UL, 0UL, 0xAUL, 0xAUL, 0x0900UL, 0x0900UL}, /* P11 */
	{0x320UL, 0UL, 0xAUL, 0xAUL, 0x0800UL, 0x0800UL}  /* P12 */
};

/* The table includes cpu px info of Intel J3455 SoC */
static const struct cpu_px_data px_j3455[] = {
	{0x5DDUL, 0UL, 0xAUL, 0xAUL, 0x1700UL, 0x1700UL}, /* P0 */
	{0x5DCUL, 0UL, 0xAUL, 0xAUL, 0x0F00UL, 0x0F00UL}, /* P1 */
	{0x578UL, 0UL, 0xAUL, 0xAUL, 0x0E00UL, 0x0E00UL}, /* P2 */
	{0x514UL, 0UL, 0xAUL, 0xAUL, 0x0D00UL, 0x0D00UL}, /* P3 */
	{0x4B0UL, 0UL, 0xAUL, 0xAUL, 0x0C00UL, 0x0C00UL}, /* P4 */
	{0x44CUL, 0UL, 0xAUL, 0xAUL, 0x0B00UL, 0x0B00UL}, /* P5 */
	{0x3E8UL, 0UL, 0xAUL, 0xAUL, 0x0A00UL, 0x0A00UL}, /* P6 */
	{0x384UL, 0UL, 0xAUL, 0xAUL, 0x0900UL, 0x0900UL}, /* P7 */
	{0x320UL, 0UL, 0xAUL, 0xAUL, 0x0800UL, 0x0800UL}  /* P8 */
};

/* The table includes cpu px info of Intel N3350 SoC */
static const struct cpu_px_data px_n3350[] = {
	{0x44DUL, 0UL, 0xAUL, 0xAUL, 0x1800UL, 0x1800UL}, /* P0 */
	{0x44CUL, 0UL, 0xAUL, 0xAUL, 0x0B00UL, 0x0B00UL}, /* P1 */
	{0x3E8UL, 0UL, 0xAUL, 0xAUL, 0x0A00UL, 0x0A00UL}, /* P2 */
	{0x384UL, 0UL, 0xAUL, 0xAUL, 0x0900UL, 0x0900UL}, /* P3 */
	{0x320UL, 0UL, 0xAUL, 0xAUL, 0x0800UL, 0x0800UL}  /* P4 */
};

/* The table includes cpu cx info of Intel J3455 SoC */
static const struct cpu_cx_data cx_j3455[] = {
	{{SPACE_FFixedHW, 0x1U, 0x2U, 0x1U, 0x01UL}, 0x1U, 0x1U, 0x3E8UL}, /* C1 */
	{{SPACE_FFixedHW, 0x1U, 0x2U, 0x1U, 0x21UL}, 0x2U, 0x32U, 0x0AUL}, /* C2 */
	{{SPACE_FFixedHW, 0x1U, 0x2U, 0x1U, 0x60UL}, 0x3U, 0x96U, 0x0AUL}  /* C3 */
};

static const struct cpu_state_table {
	char			model_name[64];
	struct cpu_state_info	state_info;
} cpu_state_tbl[] = {
	{"Intel(R) Atom(TM) Processor A3960 @ 1.90GHz",
		{(uint8_t)ARRAY_SIZE(px_a3960), px_a3960,
		 (uint8_t)ARRAY_SIZE(cx_a3960), cx_a3960}
	},
	{"Intel(R) Atom(TM) Processor A3950 @ 1.60GHz",
		{(uint8_t)ARRAY_SIZE(px_a3950), px_a3950,
		 (uint8_t)ARRAY_SIZE(cx_a3960), cx_a3960} /* Cx is same as A3960 */
	},
	{"Intel(R) Celeron(R) CPU J3455 @ 1.50GHz",
		{(uint8_t)ARRAY_SIZE(px_j3455), px_j3455,
		 (uint8_t)ARRAY_SIZE(cx_j3455), cx_j3455}
	},
	{"Intel(R) Celeron(R) CPU N3350 @ 1.10GHz",
		{(uint8_t)ARRAY_SIZE(px_n3350), px_n3350,
		 (uint8_t)ARRAY_SIZE(cx_a3960), cx_a3960} /* Cx is same as A3960 */
	}
};

static struct cpu_state_info cpu_pm_state_info;

static int32_t get_state_tbl_idx(const char *cpuname)
{
	int32_t i;
	int32_t count = ARRAY_SIZE(cpu_state_tbl);
	int32_t ret = -1;

	if (cpuname != NULL) {
		for (i = 0; i < count; i++) {
			if (strcmp((cpu_state_tbl[i].model_name), cpuname) == 0) {
				ret = i;
				break;
			}
		}
	}

	return ret;
}

struct cpu_state_info *get_cpu_pm_state_info(void)
{
	return &cpu_pm_state_info;
}

void load_cpu_state_data(void)
{
	int32_t tbl_idx;
	const struct cpu_state_info *state_info;

	(void)memset(&cpu_pm_state_info, 0U, sizeof(struct cpu_state_info));

	tbl_idx = get_state_tbl_idx(boot_cpu_data.model_name);

	if (tbl_idx >= 0) {
		/* The state table is found. */

		state_info = &(cpu_state_tbl + tbl_idx)->state_info;

		if ((state_info->px_cnt != 0U) && (state_info->px_data != NULL)) {
			if (state_info->px_cnt > MAX_PSTATE) {
				cpu_pm_state_info.px_cnt = MAX_PSTATE;
			} else {
				cpu_pm_state_info.px_cnt = state_info->px_cnt;
			}

			cpu_pm_state_info.px_data = state_info->px_data;
		}

		if ((state_info->cx_cnt != 0U) && (state_info->cx_data != NULL)) {
			if (state_info->cx_cnt > MAX_CX_ENTRY) {
				cpu_pm_state_info.cx_cnt = MAX_CX_ENTRY;
			} else {
				cpu_pm_state_info.cx_cnt = state_info->cx_cnt;
			}

			cpu_pm_state_info.cx_data = state_info->cx_data;
		}
	}
}
