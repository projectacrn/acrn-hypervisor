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

#define SBUF_MAGIC	0x5aa57aa71aa13aa3UL
#define SBUF_MAX_SIZE	(1UL << 22U)
#define SBUF_HEAD_SIZE	64U

/* sbuf flags */
#define OVERRUN_CNT_EN	(1U << 0U) /* whether overrun counting is enabled */
#define OVERWRITE_EN	(1U << 1U) /* whether overwrite is enabled */

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
	ACRN_TRACE = 0U,
	ACRN_HVLOG,
	ACRN_SEP,
	ACRN_SOCWATCH,
	ACRN_SBUF_ID_MAX,
};

/* Make sure sizeof(struct shared_buf) == SBUF_HEAD_SIZE */
struct shared_buf {
	uint64_t magic;
	uint32_t ele_num;	/* number of elements */
	uint32_t ele_size;	/* sizeof of elements */
	uint32_t head;		/* offset from base, to read */
	uint32_t tail;		/* offset from base, to write */
	uint32_t flags;
	uint32_t reserved;
	uint32_t overrun_cnt;	/* count of overrun */
	uint32_t size;		/* ele_num * ele_size */
	uint32_t padding[6];
};


/**
 *@pre sbuf != NULL
 *@pre data != NULL
 */
uint32_t sbuf_put(struct shared_buf *sbuf, uint8_t *data);
int32_t sbuf_share_setup(uint16_t pcpu_id, uint32_t sbuf_id, uint64_t *hva);
uint32_t sbuf_next_ptr(uint32_t pos, uint32_t span, uint32_t scope);

#endif /* SHARED_BUFFER_H */
