/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHARED_BUF_H
#define SHARED_BUF_H

#include <linux/types.h>

#define SBUF_MAGIC 0x5aa57aa71aa13aa3
#define SBUF_MAX_SIZE   (1ULL << 22)
#define SBUF_HEAD_SIZE  64

/* sbuf flags */
#define OVERRUN_CNT_EN  (1ULL << 0) /* whether overrun counting is enabled */
#define OVERWRITE_EN    (1ULL << 1) /* whether overwrite is enabled */

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

/**
 * (sbuf) head + buf (store (ele_num - 1) elements at most)
 * buffer empty: tail == head
 * buffer full:  (tail + ele_size) % size == head
 *
 *             Base of memory for elements
 *                |
 *                |
 * ---------------------------------------------------------------------------------------
 * | shared_buf_t | raw data (ele_size)| raw date (ele_size) | ... | raw data (ele_size) |
 * ---------------------------------------------------------------------------------------
 * |
 * |
 * shared_buf_t *buf
 */

/* Make sure sizeof(shared_buf_t) == SBUF_HEAD_SIZE */
typedef struct shared_buf {
        uint64_t magic;
        uint32_t ele_num;       /* number of elements */
        uint32_t ele_size;      /* sizeof of elements */
        uint32_t head;          /* offset from base, to read */
        uint32_t tail;          /* offset from base, to write */
        uint64_t flags;
        uint32_t overrun_cnt;   /* count of overrun */
        uint32_t size;          /* ele_num * ele_size */
        uint32_t padding[6];
} shared_buf_t;

static inline void sbuf_clear_flags(shared_buf_t *sbuf, uint64_t flags)
{
        sbuf->flags &= ~flags;
}

static inline void sbuf_set_flags(shared_buf_t *sbuf, uint64_t flags)
{
        sbuf->flags = flags;
}

static inline void sbuf_add_flags(shared_buf_t *sbuf, uint64_t flags)
{
        sbuf->flags |= flags;
}

int sbuf_get(shared_buf_t *sbuf, uint8_t *data);
int sbuf_write(int fd, shared_buf_t *sbuf);
int sbuf_clear_buffered(shared_buf_t *sbuf);
#endif /* SHARED_BUF_H */
