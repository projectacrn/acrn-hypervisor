/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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
void *alloc_page();
void *alloc_pages(unsigned int page_num);
void free(void *ptr);

#endif /* MEM_MGT_H_ */
