/*
 * SHARED BUFFER
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#include <types.h>
#include <rtl.h>
#include <errno.h>
#include <x86/cpu.h>
#include <x86/per_cpu.h>

uint32_t sbuf_next_ptr(uint32_t pos_arg,
		uint32_t span, uint32_t scope)
{
	uint32_t pos = pos_arg;
	pos += span;
	pos = (pos >= scope) ? (pos - scope) : pos;
	return pos;
}

/**
 * The high caller should guarantee each time there must have
 * sbuf->ele_size data can be write form data and this function
 * should guarantee execution atomically.
 *
 * flag:
 * If OVERWRITE_EN set, buf can store (ele_num - 1) elements at most.
 * Should use lock to guarantee that only one read or write at
 * the same time.
 * if OVERWRITE_EN not set, buf can store (ele_num - 1) elements
 * at most. Shouldn't modify the sbuf->head.
 *
 * return:
 * ele_size:	write succeeded.
 * 0:		no write, buf is full
 * negative:	failed.
 */

uint32_t sbuf_put(struct shared_buf *sbuf, uint8_t *data)
{
	void *to;
	uint32_t next_tail;
	uint32_t ele_size;
	bool trigger_overwrite = false;

	stac();
	next_tail = sbuf_next_ptr(sbuf->tail, sbuf->ele_size, sbuf->size);
	/* if this write would trigger overrun */
	if (next_tail == sbuf->head) {
		/* accumulate overrun count if necessary */
		sbuf->overrun_cnt += sbuf->flags & OVERRUN_CNT_EN;
		if ((sbuf->flags & OVERWRITE_EN) == 0U) {
			/* if not enable over write, return here. */
			clac();
			return 0;
		}
		trigger_overwrite = true;
	}

	to = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->tail;

	(void)memcpy_s(to, sbuf->ele_size, data, sbuf->ele_size);

	if (trigger_overwrite) {
		sbuf->head = sbuf_next_ptr(sbuf->head,
				sbuf->ele_size, sbuf->size);
	}
	sbuf->tail = next_tail;

	ele_size = sbuf->ele_size;
	clac();

	return ele_size;
}

int32_t sbuf_share_setup(uint16_t pcpu_id, uint32_t sbuf_id, uint64_t *hva)
{
	if ((pcpu_id >= get_pcpu_nums()) || (sbuf_id >= ACRN_SBUF_ID_MAX)) {
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
		for (sbuf_id = 0U; sbuf_id < ACRN_SBUF_ID_MAX; sbuf_id++) {
			per_cpu(sbuf, pcpu_id)[sbuf_id] = 0U;
		}
	}
}
