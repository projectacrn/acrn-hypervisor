/*
 * SHARED BUFFER
 *
 * Copyright (C) 2017-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <asm/cpu.h>
#include <per_cpu.h>
#include <cpu.h>

int32_t sbuf_share_setup(uint16_t pcpu_id, uint32_t sbuf_id, uint64_t *hva)
{
	if ((pcpu_id >= get_pcpu_nums()) || (sbuf_id >= ACRN_SBUF_PER_PCPU_ID_MAX)) {
		return -EINVAL;
	}

	per_cpu(sbuf, pcpu_id)[sbuf_id] = (struct shared_buf *) hva;
	pr_info("%s share sbuf for pCPU[%u] with sbuf_id[%u] setup successfully",
			__func__, pcpu_id, sbuf_id);

	return 0;
}

void sbuf_reset(void)
{
	uint16_t pcpu_id, sbuf_id;

	for (pcpu_id = 0U; pcpu_id < get_pcpu_nums(); pcpu_id++) {
		for (sbuf_id = 0U; sbuf_id < ACRN_SBUF_PER_PCPU_ID_MAX; sbuf_id++) {
			per_cpu(sbuf, pcpu_id)[sbuf_id] = 0U;
		}
	}
}
