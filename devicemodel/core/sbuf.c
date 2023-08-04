/*
 * Copyright (C) 2018-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include "sbuf.h"
#include <errno.h>

static inline uint32_t sbuf_next_ptr(uint32_t pos_arg,
		uint32_t span, uint32_t scope)
{
	uint32_t pos = pos_arg;
	pos += span;
	pos = (pos >= scope) ? (pos - scope) : pos;
	return pos;
}

uint32_t sbuf_get(struct shared_buf *sbuf, uint8_t *data)
{
	const void *from;

	if ((sbuf == NULL) || (data == NULL))
		return -EINVAL;

	if (sbuf_is_empty(sbuf)) {
		/* no data available */
		return 0;
	}

	from = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->head;

	memcpy(data, from, sbuf->ele_size);

	mb();

	sbuf->head = sbuf_next_ptr(sbuf->head, sbuf->ele_size, sbuf->size);

	return sbuf->ele_size;
}

int sbuf_clear_buffered(struct shared_buf *sbuf)
{
	if (sbuf == NULL)
		return -EINVAL;

	sbuf->head = sbuf->tail;

	return 0;
}

/**
 * The high caller should guarantee each time there must have
 * sbuf->ele_size data can be write form data.
 * Caller should provide the max length of the data for safety reason.
 *
 * And this function should guarantee execution atomically.
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
 * UINT32_MAX:	failed, sbuf corrupted.
 */
uint32_t sbuf_put(struct shared_buf *sbuf, uint8_t *data, uint32_t max_len)
{
	uint32_t ele_size = sbuf->ele_size;
	void *to;
	uint32_t next_tail;
	uint32_t ret;
	bool trigger_overwrite = false;

	next_tail = sbuf_next_ptr(sbuf->tail, ele_size, sbuf->size);

	if ((next_tail == sbuf->head) && ((sbuf->flags & OVERWRITE_EN) == 0U)) {
		/* if overrun is not enabled, return 0 directly */
		ret = 0U;
	} else if (ele_size <= max_len) {
		if (next_tail == sbuf->head) {
			/* accumulate overrun count if necessary */
			sbuf->overrun_cnt += sbuf->flags & OVERRUN_CNT_EN;
			trigger_overwrite = true;
		}
		to = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->tail;

		memcpy(to, data, ele_size);
		/* make sure write data before update head */
		mb();

		if (trigger_overwrite) {
			sbuf->head = sbuf_next_ptr(sbuf->head,
					ele_size, sbuf->size);
		}
		sbuf->tail = next_tail;
		ret = ele_size;
	} else {
		/* there must be something wrong */
		ret = UINT32_MAX;
	}

	return ret;
}

void sbuf_init(struct shared_buf *sbuf, uint32_t total_size, uint32_t ele_size)
{
	sbuf->magic = SBUF_MAGIC;
	sbuf->ele_size = ele_size;
	sbuf->ele_num = (total_size - SBUF_HEAD_SIZE) / sbuf->ele_size;
	sbuf->size = sbuf->ele_size * sbuf->ele_num;
	sbuf->flags = 0;
	sbuf->overrun_cnt = 0;
	sbuf->head = 0;
	sbuf->tail = 0;
}
