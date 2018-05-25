/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/errno.h>
#include <string.h>
#include <stdbool.h>
#include "sbuf.h"

static inline bool sbuf_is_empty(shared_buf_t *sbuf)
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

int sbuf_get(shared_buf_t *sbuf, uint8_t *data)
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

	sbuf->head = sbuf_next_ptr(sbuf->head, sbuf->ele_size, sbuf->size);

	return sbuf->ele_size;
}

int sbuf_clear_buffered(shared_buf_t *sbuf)
{
	if (sbuf == NULL)
		return -EINVAL;

	sbuf->head = sbuf->tail;

	return 0;
}
