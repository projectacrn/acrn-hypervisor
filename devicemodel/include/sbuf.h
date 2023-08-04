/*
 * Copyright (C) 2018-2023 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHARED_BUF_H
#define SHARED_BUF_H

#include <linux/types.h>
#include "acrn_common.h"


static inline bool sbuf_is_empty(struct shared_buf *sbuf)
{
	return (sbuf->head == sbuf->tail);
}

static inline void sbuf_clear_flags(struct shared_buf *sbuf, uint64_t flags)
{
        sbuf->flags &= ~flags;
}

static inline void sbuf_set_flags(struct shared_buf *sbuf, uint64_t flags)
{
        sbuf->flags = flags;
}

static inline void sbuf_add_flags(struct shared_buf *sbuf, uint64_t flags)
{
        sbuf->flags |= flags;
}

uint32_t sbuf_get(struct shared_buf *sbuf, uint8_t *data);
uint32_t sbuf_put(struct shared_buf *sbuf, uint8_t *data, uint32_t max_len);
int sbuf_clear_buffered(struct shared_buf *sbuf);
void sbuf_init(struct shared_buf *sbuf, uint32_t total_size, uint32_t ele_size);

#endif /* SHARED_BUF_H */
