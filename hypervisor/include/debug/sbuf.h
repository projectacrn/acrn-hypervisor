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

#ifndef SHARED_BUFFER_H
#define SHARED_BUFFER_H

#define SBUF_MAGIC	0x5aa57aa71aa13aa3
#define SBUF_MAX_SIZE	(1 << 22)
#define SBUF_HEAD_SIZE	64

/* sbuf flags */
#define OVERRUN_CNT_EN	(1 << 0) /* whether overrun counting is enabled */
#define OVERWRITE_EN	(1 << 1) /* whether overwrite is enabled */

/**
 * (sbuf) head + buf (store (ele_num - 1) elements at most)
 * buffer empty: tail == head
 * buffer full:  (tail + ele_size) % size == head
 *
 *             Base of memory for elements
 *                |
 *                |
 * ----------------------------------------------------------------------
 * | struct shared_buf | raw data (ele_size)| ... | raw data (ele_size) |
 * ----------------------------------------------------------------------
 * |
 * |
 * struct shared_buf *buf
 */

enum {
	ACRN_TRACE,
	ACRN_HVLOG,
	ACRN_SBUF_ID_MAX,
};

/* Make sure sizeof(struct shared_buf) == SBUF_HEAD_SIZE */
struct shared_buf {
	uint64_t magic;
	uint32_t ele_num;	/* number of elements */
	uint32_t ele_size;	/* sizeof of elements */
	uint32_t head;		/* offset from base, to read */
	uint32_t tail;		/* offset from base, to write */
	uint64_t flags;
	uint32_t overrun_cnt;	/* count of overrun */
	uint32_t size;		/* ele_num * ele_size */
	uint32_t padding[6];
};

#ifdef HV_DEBUG

EXTERN_CPU_DATA(uint64_t * [ACRN_SBUF_ID_MAX], sbuf);

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

struct shared_buf *sbuf_allocate(uint32_t ele_num, uint32_t ele_size);
void sbuf_free(struct shared_buf *sbuf);
int sbuf_get(struct shared_buf *sbuf, uint8_t *data);
int sbuf_put(struct shared_buf *sbuf, uint8_t *data);
int sbuf_share_setup(uint32_t pcpu_id, uint32_t sbuf_id, uint64_t *hva);

#else /* HV_DEBUG */

static inline void sbuf_clear_flags(
		__unused struct shared_buf *sbuf,
		__unused uint64_t flags)
{
}

static inline void sbuf_set_flags(
		__unused struct shared_buf *sbuf,
		__unused uint64_t flags)
{
}

static inline void sbuf_add_flags(
		__unused struct shared_buf *sbuf,
		__unused uint64_t flags)
{
}

static inline struct shared_buf *sbuf_allocate(
		__unused uint32_t ele_num,
		__unused uint32_t ele_size)
{
	return NULL;
}

static inline void sbuf_free(
		__unused struct shared_buf *sbuf)
{
}

static inline int sbuf_get(
		__unused struct shared_buf *sbuf,
		__unused uint8_t *data)
{
	return 0;
}

static inline int sbuf_put(
		__unused struct shared_buf *sbuf,
		__unused uint8_t *data)
{
	return 0;
}

static inline int sbuf_share_setup(
		__unused uint32_t pcpu_id,
		__unused uint32_t sbuf_id,
		__unused uint64_t *hva)
{
	return -1;
}
#endif /* HV_DEBUG */

#endif /* SHARED_BUFFER_H */
