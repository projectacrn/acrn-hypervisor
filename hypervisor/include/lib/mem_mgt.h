/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MEM_MGT_H__
#define __MEM_MGT_H__

/* Macros */
#define BITMAP_WORD_SIZE         32

struct mem_pool {
	void *start_addr;	/* Start Address of Memory Pool */
	spinlock_t spinlock;	/* To protect Memory Allocation */
	uint32_t size;		/* Size of Memory Pool in Bytes */
	uint32_t buff_size;	/* Size of one Buffer in Bytes */
	uint32_t total_buffs;	/* Total Buffers in Memory Pool */
	uint32_t bmp_size;	/* Size of Bitmap Array */
	uint32_t *bitmap;		/* Pointer to allocation bitmap */
	uint32_t *contiguity_bitmap;	/* Pointer to contiguity bitmap */
};

/* APIs exposing memory allocation/deallocation abstractions */
void *malloc(unsigned int num_bytes);
void *calloc(unsigned int num_elements, unsigned int element_size);
void *alloc_page(void);
void *alloc_pages(unsigned int page_num);
void free(void *ptr);

#endif /* MEM_MGT_H_ */
