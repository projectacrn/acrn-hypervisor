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

#include <hypervisor.h>

static inline bool sbuf_is_empty(struct shared_buf *sbuf)
{
	return (sbuf->head == sbuf->tail);
}

static inline uint32_t sbuf_next_ptr(uint32_t pos,
		uint32_t span, uint32_t scope)
{
	pos += span;
	pos = (pos >= scope) ? (pos - scope) : pos;
	return pos;
}

static inline uint32_t sbuf_calculate_allocate_size(uint32_t ele_num,
						uint32_t ele_size)
{
	uint64_t sbuf_allocate_size;

	sbuf_allocate_size = ele_num * ele_size;
	sbuf_allocate_size +=  SBUF_HEAD_SIZE;
	if (sbuf_allocate_size > SBUF_MAX_SIZE) {
		pr_err("%s, num=0x%x, size=0x%x exceed 0x%x",
			__func__, ele_num, ele_size, SBUF_MAX_SIZE);
		return 0;
	}

	return (uint32_t) sbuf_allocate_size;
}

struct shared_buf *sbuf_allocate(uint32_t ele_num, uint32_t ele_size)
{
	struct shared_buf *sbuf;
	uint32_t sbuf_allocate_size;

	if (ele_num == 0U || ele_size == 0U) {
		pr_err("%s invalid parameter!", __func__);
		return NULL;
	}

	sbuf_allocate_size = sbuf_calculate_allocate_size(ele_num, ele_size);
	if (sbuf_allocate_size == 0U)
		return NULL;

	sbuf = calloc(1, sbuf_allocate_size);
	if (sbuf == NULL) {
		pr_err("%s no memory!", __func__);
		return NULL;
	}

	sbuf->ele_num = ele_num;
	sbuf->ele_size = ele_size;
	sbuf->size = ele_num * ele_size;
	sbuf->magic = SBUF_MAGIC;
	pr_info("%s ele_num=0x%x, ele_size=0x%x allocated",
			__func__, ele_num, ele_size);
	return sbuf;
}

void sbuf_free(struct shared_buf *sbuf)
{
	if ((sbuf == NULL) || sbuf->magic != SBUF_MAGIC) {
		pr_err("%s invalid parameter!", __func__);
		return;
	}

	sbuf->magic = 0UL;
	free(sbuf);
}

int sbuf_get(struct shared_buf *sbuf, uint8_t *data)
{
	const void *from;

	if ((sbuf == NULL) || (data == NULL))
		return -EINVAL;

	if (sbuf_is_empty(sbuf)) {
		/* no data available */
		return 0;
	}

	from = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->head;

	memcpy_s((void *)data, sbuf->ele_size, from, sbuf->ele_size);

	sbuf->head = sbuf_next_ptr(sbuf->head, sbuf->ele_size, sbuf->size);

	return sbuf->ele_size;
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

int sbuf_put(struct shared_buf *sbuf, uint8_t *data)
{
	void *to;
	uint32_t next_tail;
	bool trigger_overwrite = false;

	if ((sbuf == NULL) || (data == NULL))
		return -EINVAL;

	next_tail = sbuf_next_ptr(sbuf->tail, sbuf->ele_size, sbuf->size);
	/* if this write would trigger overrun */
	if (next_tail == sbuf->head) {
		/* accumulate overrun count if necessary */
		sbuf->overrun_cnt += sbuf->flags & OVERRUN_CNT_EN;
		if ((sbuf->flags & OVERWRITE_EN) == 0U) {
			/* if not enable over write, return here. */
			return 0;
		}
		trigger_overwrite = true;
	}

	to = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->tail;

	memcpy_s(to, sbuf->ele_size, data, sbuf->ele_size);

	if (trigger_overwrite) {
		sbuf->head = sbuf_next_ptr(sbuf->head,
				sbuf->ele_size, sbuf->size);
	}
	sbuf->tail = next_tail;

	return sbuf->ele_size;
}

int sbuf_share_setup(uint16_t pcpu_id, uint32_t sbuf_id, uint64_t *hva)
{
	if (pcpu_id >= phys_cpu_num ||
			sbuf_id >= ACRN_SBUF_ID_MAX)
		return -EINVAL;

	per_cpu(sbuf, pcpu_id)[sbuf_id] = hva;
	pr_info("%s share sbuf for pCPU[%u] with sbuf_id[%u] setup successfully",
			__func__, pcpu_id, sbuf_id);

	return 0;
}
