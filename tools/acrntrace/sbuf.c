/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

int sbuf_write(int fd, shared_buf_t *sbuf)
{
	const void *start;
	int written;

	if (sbuf == NULL)
		return -EINVAL;

	if (sbuf_is_empty(sbuf)) {
		return 0;
	}

	start = (void *)sbuf + SBUF_HEAD_SIZE + sbuf->head;
        written = write(fd, start, sbuf->ele_size);
	if (written != sbuf->ele_size) {
		printf("Failed to write: ret %d (ele_size %d), errno %d\n",
			written, sbuf->ele_size, (written == -1) ? errno : 0);
		return -1;
	}

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
