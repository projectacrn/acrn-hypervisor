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

#include <hv_lib.h>
#include <cpu.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <cpu_state_tbl.h>

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

struct cpu_state_table cpu_state_tbl[] = {
	{"Intel(R) Atom(TM) Processor A3960 @ 1.90GHz", 17, px_a3960}
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

	boot_cpu_data.px_cnt = 0;
	boot_cpu_data.px_data = NULL;

	tbl_idx = get_state_tbl_idx(boot_cpu_data.model_name);
	if (tbl_idx < 0) {
		/* The state table is not found. */
		return;
	}

	if (!((cpu_state_tbl + tbl_idx)->px_cnt)
		|| !((cpu_state_tbl + tbl_idx)->px_data)) {
		/* The state table must be wrong. */
		return;
	}

	if ((cpu_state_tbl + tbl_idx)->px_cnt > MAX_PSTATE) {
		boot_cpu_data.px_cnt = MAX_PSTATE;
	} else {
		boot_cpu_data.px_cnt = (cpu_state_tbl + tbl_idx)->px_cnt;
	}

	boot_cpu_data.px_data = (cpu_state_tbl + tbl_idx)->px_data;

}

int validate_pstate(struct vm *vm, uint64_t perf_ctl)
{
	struct cpu_px_data *px_data;
	int i, px_cnt;

	if (is_vm0(vm)) {
		return 0;
	}

	px_cnt = vm->pm.px_cnt;
	px_data = vm->pm.px_data;

	if (!px_cnt || !px_data) {
		return -1;
	}

	for (i = 0; i < px_cnt; i++) {
		if ((px_data + i)->control == (perf_ctl & 0xffff)) {
			return 0;
		}
	}

	return -1;
}

void vm_setup_cpu_px(struct vm *vm)
{
	uint32_t px_data_size;

	vm->pm.px_cnt = 0;
	memset(vm->pm.px_data, 0, MAX_PSTATE * sizeof(struct cpu_px_data));

	if ((!boot_cpu_data.px_cnt) || (!boot_cpu_data.px_data)) {
		return;
	}

	if (boot_cpu_data.px_cnt > MAX_PSTATE) {
		vm->pm.px_cnt = MAX_PSTATE;
	} else {
		vm->pm.px_cnt = boot_cpu_data.px_cnt;
	}

	px_data_size = vm->pm.px_cnt * sizeof(struct cpu_px_data);

	memcpy_s(vm->pm.px_data, px_data_size,
			boot_cpu_data.px_data, px_data_size);

}

void vm_setup_cpu_state(struct vm *vm)
{
	vm_setup_cpu_px(vm);
}
