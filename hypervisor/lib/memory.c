/*
 * Copyright (C) 2018 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/************************************************************************/
/*  Memory pool declaration (block size = CONFIG_MALLOC_ALIGN)   */
/************************************************************************/
#define __bss_noinit __attribute__((__section__(".bss_noinit")))

static uint8_t __bss_noinit
Malloc_Heap[CONFIG_HEAP_SIZE] __aligned(CONFIG_MALLOC_ALIGN);

#define MALLOC_HEAP_BUFF_SIZE      CONFIG_MALLOC_ALIGN
#define MALLOC_HEAP_TOTAL_BUFF     (CONFIG_HEAP_SIZE/MALLOC_HEAP_BUFF_SIZE)
#define MALLOC_HEAP_BITMAP_SIZE \
	INT_DIV_ROUNDUP(MALLOC_HEAP_TOTAL_BUFF, BITMAP_WORD_SIZE)
static uint32_t Malloc_Heap_Bitmap[MALLOC_HEAP_BITMAP_SIZE];
static uint32_t Malloc_Heap_Contiguity_Bitmap[MALLOC_HEAP_BITMAP_SIZE];

static struct mem_pool Memory_Pool = {
	.start_addr = Malloc_Heap,
	.spinlock = {.head = 0U, .tail = 0U},
	.size = CONFIG_HEAP_SIZE,
	.buff_size = MALLOC_HEAP_BUFF_SIZE,
	.total_buffs = MALLOC_HEAP_TOTAL_BUFF,
	.bmp_size = MALLOC_HEAP_BITMAP_SIZE,
	.bitmap = Malloc_Heap_Bitmap,
	.contiguity_bitmap = Malloc_Heap_Contiguity_Bitmap
};

static void *allocate_mem(struct mem_pool *pool, uint32_t num_bytes)
{

	void *memory = NULL;
	uint32_t idx;
	uint16_t bit_idx;
	uint32_t requested_buffs;

	/* Check if provided memory pool exists */
	if (pool == NULL) {
		return NULL;
	}

	/* Acquire the pool lock */
	spinlock_obtain(&pool->spinlock);

	/* Calculate number of buffers to be allocated from memory pool */
	requested_buffs = INT_DIV_ROUNDUP(num_bytes, pool->buff_size);

	for (idx = 0U; idx < pool->bmp_size; idx++) {
		/* Find the first occurrence of requested_buffs number of free
		 * buffers. The 0th bit in bitmap represents a free buffer.
		 */
		for (bit_idx = ffz64(pool->bitmap[idx]);
					bit_idx < BITMAP_WORD_SIZE; bit_idx++) {
			/* Check if selected buffer is free */
			if ((pool->bitmap[idx] & (1U << bit_idx)) != 0U) {
				continue;
			}

			/* Declare temporary variables to be used locally in
			 * this block
			 */
			uint32_t i;
			uint16_t tmp_bit_idx = bit_idx;
			uint32_t tmp_idx = idx;

			/* Check requested_buffs number of buffers availability
			 * in memory-pool right after selected buffer
			 */
			for (i = 1U; i < requested_buffs; i++) {
				/* Check if tmp_bit_idx is out-of-range */
				tmp_bit_idx++;
				if (tmp_bit_idx == BITMAP_WORD_SIZE) {
					/* Break the loop if tmp_idx is
					 * out-of-range
					 */
					tmp_idx++;
					if (tmp_idx == pool->bmp_size) {
						break;
					}
					/* Reset tmp_bit_idx */
					tmp_bit_idx = 0U;
				}

				/* Break if selected buffer is not free */
				if ((pool->bitmap[tmp_idx]
					& (1U << tmp_bit_idx)) != 0U) {
					break;
				}
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
					(pool->buff_size *
					((idx * BITMAP_WORD_SIZE) +
							bit_idx));

				/* Update allocation bitmaps information for
				 * selected buffers
				 */
				for (i = 0U; i < requested_buffs; i++) {
					/* Set allocation bit in bitmap for
					 * this buffer
					 */
					pool->bitmap[idx] |= (1U << bit_idx);

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
							(1U << bit_idx);
					} else {
						/* Set contiguity bit to 0 if
						 * this buffer is not the last
						 * of selected contiguous
						 * buffers array
						 */
						pool->contiguity_bitmap[idx] &=
							~(1U << bit_idx);
					}

					/* Check if bit_idx is out-of-range */
					bit_idx++;
					if (bit_idx == BITMAP_WORD_SIZE) {
						/* Increment idx */
						idx++;
						/* Reset bit_idx */
						bit_idx = 0U;
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

static void deallocate_mem(struct mem_pool *pool, const void *ptr)
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
			if ((*bitmask & (1U << bit_idx)) != 0U) {
				*bitmask ^= (1U << bit_idx);
			} else {
				break;
			}

			/* Reset the Contiguity bit of buffer */
			if ((*contiguity_bitmask & (1U << bit_idx)) != 0U) {
				*contiguity_bitmask ^= (1U << bit_idx);
			} else {
				break;
			}

			/* Increment buff_idx */
			buff_idx++;
		}

		/* Release the pool lock. */
		spinlock_release(&pool->spinlock);
	}
}

/*
 * The return address will be PAGE_SIZE aligned if 'num_bytes' is greater
 * than PAGE_SIZE.
 */
void *malloc(uint32_t num_bytes)
{
	void *memory = NULL;

	/* Check if bytes requested extend page-size */
	if (num_bytes < PAGE_SIZE) {
		/*
		 * Request memory allocation from smaller segmented memory pool
		 */
		memory = allocate_mem(&Memory_Pool, num_bytes);
	}

	/* Check if memory allocation is successful */
	if (memory == NULL) {
		pr_err("%s: failed to alloc 0x%x Bytes", __func__, num_bytes);
	}

	/* Return memory pointer to caller */
	return memory;
}

void *calloc(uint32_t num_elements, uint32_t element_size)
{
	void *memory = malloc(num_elements * element_size);

	/* Determine if memory was allocated */
	if (memory != NULL) {
		/* Zero all the memory */
		(void)memset(memory, 0U, num_elements * element_size);
	}

	/* Return pointer to memory */
	return memory;
}

void free(const void *ptr)
{
	/* Check if ptr belongs to 16-Bytes aligned Memory Pool */
	if ((Memory_Pool.start_addr < ptr) &&
		(ptr < (Memory_Pool.start_addr +
			(Memory_Pool.total_buffs * Memory_Pool.buff_size)))) {
		/* Free buffer in 16-Bytes aligned Memory Pool */
		deallocate_mem(&Memory_Pool, ptr);
	}
}

void *memchr(const void *void_s, int32_t c, size_t n)
{
	uint8_t val = (uint8_t)c;
	uint8_t *ptr = (uint8_t *)void_s;
	uint8_t *end = ptr + n;

	while (ptr < end) {
		if (*ptr == val) {
			return ((void *)ptr);
		}
		ptr++;
	}
	return NULL;
}

static inline void memcpy_erms(void *d, const void *s, size_t slen)
{
	asm volatile ("rep; movsb"
		: "=&D"(d), "=&S"(s)
		: "c"(slen), "0" (d), "1" (s)
		: "memory");
}

/*
 * @brief  Copies at most slen bytes from src address to dest address, up to dmax.
 *
 *   INPUTS
 *
 * @param[in] d        pointer to Destination address
 * @param[in] dmax     maximum  length of dest
 * @param[in] s        pointer to Source address
 * @param[in] slen     maximum number of bytes of src to copy
 *
 * @return pointer to destination address.
 *
 * @pre d and s will not overlap.
 */
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen)
{
	if ((slen != 0U) && (dmax != 0U) && (dmax >= slen)) {
		/* same memory block, no need to copy */
		if (d != s) {
			memcpy_erms(d, s, slen);
		}
	}
	return d;
}

static inline void memset_erms(void *base, uint8_t v, size_t n)
{
	asm volatile("rep ; stosb"
			: "+D"(base)
			: "a" (v), "c"(n));
}

void *memset(void *base, uint8_t v, size_t n)
{
	/*
	 * Some CPUs support enhanced REP MOVSB/STOSB feature. It is recommended
	 * to use it when possible.
	 */
	if ((base != NULL) && (n != 0U)) {
		memset_erms(base, v, n);
        }

	return base;
}
