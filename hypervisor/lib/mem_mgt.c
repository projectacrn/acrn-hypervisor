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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

/************************************************************************/
/*  Memory pool declaration (block size = MALLOC_ALIGN)   */
/************************************************************************/
#define __bss_noinit __attribute__((__section__(".bss_noinit")))

static uint8_t __bss_noinit Malloc_Heap[HEAP_SIZE] __aligned(MALLOC_ALIGN);

#define MALLOC_HEAP_BUFF_SIZE      MALLOC_ALIGN
#define MALLOC_HEAP_TOTAL_BUFF     (HEAP_SIZE/MALLOC_HEAP_BUFF_SIZE)
#define MALLOC_HEAP_BITMAP_SIZE	\
		INT_DIV_ROUNDUP(MALLOC_HEAP_TOTAL_BUFF, BITMAP_WORD_SIZE)
static uint32_t Malloc_Heap_Bitmap[MALLOC_HEAP_BITMAP_SIZE];
static uint32_t Malloc_Heap_Contiguity_Bitmap[MALLOC_HEAP_BITMAP_SIZE];

struct mem_pool Memory_Pool = {
	.start_addr = Malloc_Heap,
	.spinlock = {.head = 0, .tail = 0},
	.size = HEAP_SIZE,
	.buff_size = MALLOC_HEAP_BUFF_SIZE,
	.total_buffs = MALLOC_HEAP_TOTAL_BUFF,
	.bmp_size = MALLOC_HEAP_BITMAP_SIZE,
	.bitmap = Malloc_Heap_Bitmap,
	.contiguity_bitmap = Malloc_Heap_Contiguity_Bitmap
};

/************************************************************************/
/*        Memory pool declaration (block size = CPU_PAGE_SIZE)          */
/************************************************************************/
static uint8_t __bss_noinit
Paging_Heap[NUM_ALLOC_PAGES][CPU_PAGE_SIZE] __aligned(CPU_PAGE_SIZE);

#define PAGING_HEAP_BUFF_SIZE      CPU_PAGE_SIZE
#define PAGING_HEAP_TOTAL_BUFF     NUM_ALLOC_PAGES
#define PAGING_HEAP_BITMAP_SIZE	\
		INT_DIV_ROUNDUP(PAGING_HEAP_TOTAL_BUFF, BITMAP_WORD_SIZE)
static uint32_t Paging_Heap_Bitmap[PAGING_HEAP_BITMAP_SIZE];
static uint32_t Paging_Heap_Contiguity_Bitmap[MALLOC_HEAP_BITMAP_SIZE];

struct mem_pool Paging_Memory_Pool = {
	.start_addr = Paging_Heap,
	.spinlock = {.head = 0, .tail = 0},
	.size = NUM_ALLOC_PAGES * CPU_PAGE_SIZE,
	.buff_size = PAGING_HEAP_BUFF_SIZE,
	.total_buffs = PAGING_HEAP_TOTAL_BUFF,
	.bmp_size = PAGING_HEAP_BITMAP_SIZE,
	.bitmap = Paging_Heap_Bitmap,
	.contiguity_bitmap = Paging_Heap_Contiguity_Bitmap
};

static void *allocate_mem(struct mem_pool *pool, unsigned int num_bytes)
{

	void *memory = NULL;
	uint32_t idx, bit_idx;
	uint32_t requested_buffs;

	/* Check if provided memory pool exists */
	if (pool == NULL)
		return NULL;

	/* Acquire the pool lock */
	spinlock_obtain(&pool->spinlock);

	/* Calculate number of buffers to be allocated from memory pool */
	requested_buffs = INT_DIV_ROUNDUP(num_bytes, pool->buff_size);

	for (idx = 0; idx < pool->bmp_size; idx++) {
		/* Find the first occurrence of requested_buffs number of free
		 * buffers. The 0th bit in bitmap represents a free buffer.
		 */
		for (bit_idx = get_first_zero_bit(pool->bitmap[idx]);
		     bit_idx < BITMAP_WORD_SIZE; bit_idx++) {
			/* Check if selected buffer is free */
			if (pool->bitmap[idx] & (1 << bit_idx))
				continue;

			/* Declare temporary variables to be used locally in
			 * this block
			 */
			uint32_t i;
			uint32_t tmp_bit_idx = bit_idx;
			uint32_t tmp_idx = idx;

			/* Check requested_buffs number of buffers availability
			 * in memory-pool right after selected buffer
			 */
			for (i = 1; i < requested_buffs; i++) {
				/* Check if tmp_bit_idx is out-of-range */
				if (++tmp_bit_idx == BITMAP_WORD_SIZE) {
					/* Break the loop if tmp_idx is
					 * out-of-range
					 */
					if (++tmp_idx == pool->bmp_size)
						break;
					/* Reset tmp_bit_idx */
					tmp_bit_idx = 0;
				}

				/* Break if selected buffer is not free */
				if (pool->bitmap[tmp_idx] & (1 << tmp_bit_idx))
					break;
			}

			/* Check if requested_buffs number of free contiguous
			 * buffers are found in memory pool
			 */
			if (i == requested_buffs) {
				/* Get start address of first buffer among
				 * selected free contiguous buffer in the
				 * memory pool
				 */
				memory = (char *)pool->start_addr +
				    pool->buff_size * (idx * BITMAP_WORD_SIZE +
						       bit_idx);

				/* Update allocation bitmaps information for
				 * selected buffers
				 */
				for (i = 0; i < requested_buffs; i++) {
					/* Set allocation bit in bitmap for
					 * this buffer
					 */
					pool->bitmap[idx] |= (1 << bit_idx);

					/* Set contiguity information for this
					 * buffer in contiguity-bitmap
					 */
					if (i < (requested_buffs - 1)) {
						/* Set contiguity bit to 1 if
						 * this buffer is not the last
						 * of selected contiguous
						 * buffers array
						 */
						pool->contiguity_bitmap[idx] |=
						    (1 << bit_idx);
					} else {
						/* Set contiguity bit to 0 if
						 * this buffer is not the last
						 * of selected contiguous
						 * buffers array
						 */
						pool->contiguity_bitmap[idx] &=
						    ~(1 << bit_idx);
					}

					/* Check if bit_idx is out-of-range */
					if (++bit_idx == BITMAP_WORD_SIZE) {
						/* Increment idx */
						idx++;
						/* Reset bit_idx */
						bit_idx = 0;
					}
				}

				/* Release the pool lock. */
				spinlock_release(&pool->spinlock);

				return memory;
			}
			/* Update bit_idx and idx */
			bit_idx = tmp_bit_idx;
			idx = tmp_idx;
		}
	}

	/* Release the pool lock. */
	spinlock_release(&pool->spinlock);

	return (void *)NULL;
}

static void deallocate_mem(struct mem_pool *pool, void *ptr)
{
	uint32_t *bitmask, *contiguity_bitmask;
	uint32_t bmp_idx, bit_idx, buff_idx;

	if ((pool != NULL) && (ptr != NULL)) {
		/* Acquire the pool lock */
		spinlock_obtain(&pool->spinlock);

		/* Map the buffer address to its index. */
		buff_idx = ((char *)ptr - (char *)pool->start_addr) /
		    pool->buff_size;

		/* De-allocate all allocated contiguous memory buffers */
		while (buff_idx < pool->total_buffs) {
			/* Translate the buffer index to bitmap index. */
			bmp_idx = buff_idx / BITMAP_WORD_SIZE;
			bit_idx = buff_idx % BITMAP_WORD_SIZE;

			/* Get bitmap's reference for this buffer */
			bitmask = &pool->bitmap[bmp_idx];
			contiguity_bitmask = &pool->contiguity_bitmap[bmp_idx];

			/* Mark the buffer as free */
			if (*bitmask & (1 << bit_idx))
				*bitmask ^= (1 << bit_idx);
			else
				break;

			/* Reset the Contiguity bit of buffer */
			if (*contiguity_bitmask & (1 << bit_idx))
				*contiguity_bitmask ^= (1 << bit_idx);
			else
				break;

			/* Increment buff_idx */
			buff_idx++;
		}

		/* Release the pool lock. */
		spinlock_release(&pool->spinlock);
	}
}

void *malloc(unsigned int num_bytes)
{
	void *memory = NULL;

	/* Check if bytes requested extend page-size */
	if (num_bytes < CPU_PAGE_SIZE) {
		/* Request memory allocation from smaller segmented memory pool
		 */
		memory = allocate_mem(&Memory_Pool, num_bytes);
	} else {
		int page_num =
			(num_bytes + CPU_PAGE_SIZE - 1) >> CPU_PAGE_SHIFT;
		/* Request memory allocation through alloc_page */
		memory = alloc_pages(page_num);
	}

	/* Check if memory allocation is successful */
	if (memory == NULL)
		pr_err("%s: failed to alloc 0x%x Bytes", __func__, num_bytes);

	/* Return memory pointer to caller */
	return memory;
}

void *alloc_pages(unsigned int page_num)
{
	void *memory = NULL;

	/* Request memory allocation from Page-aligned memory pool */
	memory = allocate_mem(&Paging_Memory_Pool, page_num * CPU_PAGE_SIZE);

	/* Check if memory allocation is successful */
	if (memory == NULL)
		pr_err("%s: failed to alloc %d pages", __func__, page_num);

	return memory;
}

void *alloc_page(void)
{
	return alloc_pages(1);
}

void *calloc(unsigned int num_elements, unsigned int element_size)
{
	void *memory = malloc(num_elements * element_size);

	/* Determine if memory was allocated */
	if (memory != NULL) {
		/* Zero all the memory */
		memset(memory, 0, num_elements * element_size);
	}

	/* Return pointer to memory */
	return memory;
}

void free(void *ptr)
{
	/* Check if ptr belongs to 16-Bytes aligned Memory Pool */
	if ((Memory_Pool.start_addr < ptr) &&
	    (ptr < (Memory_Pool.start_addr +
		    (Memory_Pool.total_buffs * Memory_Pool.buff_size)))) {
		/* Free buffer in 16-Bytes aligned Memory Pool */
		deallocate_mem(&Memory_Pool, ptr);
	}
	/* Check if ptr belongs to page aligned Memory Pool */
	else if ((Paging_Memory_Pool.start_addr < ptr) &&
		 (ptr < (Paging_Memory_Pool.start_addr +
			 (Paging_Memory_Pool.total_buffs *
			  Paging_Memory_Pool.buff_size)))) {
		/* Free buffer in page aligned Memory Pool */
		deallocate_mem(&Paging_Memory_Pool, ptr);
	}
}
