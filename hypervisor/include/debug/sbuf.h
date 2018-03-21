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
