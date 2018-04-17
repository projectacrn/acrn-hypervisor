/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <acrn_common.h>
#include <hv_lib.h>
#include <hv_arch.h>

/* The table includes cpu px info of Intel A3960 SoC */
struct cpu_px_data px_a3960[] = {
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
struct cpu_cx_data cx_a3960[] = {
	{{SPACE_FFixedHW,  0x0, 0, 0,     0}, 0x1, 0x1, 0x3E8}, /* C1 */
	{{SPACE_SYSTEM_IO, 0x8, 0, 0, 0x415}, 0x2, 0x32, 0x0A}, /* C2 */
	{{SPACE_SYSTEM_IO, 0x8, 0, 0, 0x419}, 0x3, 0x96, 0x0A}  /* C3 */
};

/* The table includes cpu px info of Intel J3455 SoC */
struct cpu_px_data px_j3455[] = {
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

struct cpu_state_table {
	char			model_name[64];
	struct cpu_state_info	state_info;
} cpu_state_tbl[] = {
	{"Intel(R) Atom(TM) Processor A3960 @ 1.90GHz",
		{ARRAY_SIZE(px_a3960), px_a3960,
		 ARRAY_SIZE(cx_a3960), cx_a3960}
	},
	{"Intel(R) Celeron(R) CPU J3455 @ 1.50GHz",
		{ARRAY_SIZE(px_j3455), px_j3455,
			0,	NULL}
	}
};

static int get_state_tbl_idx(char *cpuname)
{
	int i;
	int count = ARRAY_SIZE(cpu_state_tbl);

	if (!cpuname) {
		return -1;
	}

	for (i = 0; i < count; i++) {
		if (!strcmp((cpu_state_tbl[i].model_name),
				cpuname)) {
			return i;
		}
	}

	return -1;
}

void load_cpu_state_data(void)
{
	int tbl_idx;
	struct cpu_state_info *state_info;

	memset(&boot_cpu_data.state_info, 0,
			sizeof(struct cpu_state_info));

	tbl_idx = get_state_tbl_idx(boot_cpu_data.model_name);
	if (tbl_idx < 0) {
		/* The state table is not found. */
		return;
	}

	state_info = &(cpu_state_tbl + tbl_idx)->state_info;

	if (state_info->px_cnt && state_info->px_data) {
		if (state_info->px_cnt > MAX_PSTATE) {
			boot_cpu_data.state_info.px_cnt = MAX_PSTATE;
		} else {
			boot_cpu_data.state_info.px_cnt = state_info->px_cnt;
		}

		boot_cpu_data.state_info.px_data = state_info->px_data;
	}

	if (state_info->cx_cnt && state_info->cx_data) {
		if (state_info->cx_cnt > MAX_CX_ENTRY) {
			boot_cpu_data.state_info.cx_cnt = MAX_CX_ENTRY;
		} else {
			boot_cpu_data.state_info.cx_cnt = state_info->cx_cnt;
		}

		boot_cpu_data.state_info.cx_data = state_info->cx_data;
	}

}
