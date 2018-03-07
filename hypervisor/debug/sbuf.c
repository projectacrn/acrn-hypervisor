/*
 * SHARED BUFFER
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
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
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

DEFINE_CPU_DATA(uint64_t * [ACRN_SBUF_ID_MAX], sbuf);

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

	return sbuf_allocate_size;
}

struct shared_buf *sbuf_allocate(uint32_t ele_num, uint32_t ele_size)
{
	struct shared_buf *sbuf;
	uint32_t sbuf_allocate_size;

	if (!ele_num || !ele_size) {
		pr_err("%s invalid parameter!", __func__);
		return NULL;
	}

	sbuf_allocate_size = sbuf_calculate_allocate_size(ele_num, ele_size);
	if (!sbuf_allocate_size)
		return NULL;

	sbuf = malloc(sbuf_allocate_size);
	if (sbuf == NULL) {
		pr_err("%s no memory!", __func__);
		return NULL;
	}

	memset(sbuf, 0, SBUF_HEAD_SIZE);
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

	sbuf->magic = 0;
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
		if (!(sbuf->flags & OVERWRITE_EN)) {
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

int sbuf_share_setup(uint32_t pcpu_id, uint32_t sbuf_id, uint64_t *hva)
{
	if (pcpu_id >= (uint32_t) phy_cpu_num ||
			sbuf_id >= ACRN_SBUF_ID_MAX)
		return -EINVAL;

	per_cpu(sbuf, pcpu_id)[sbuf_id] = hva;
	return 0;
}
