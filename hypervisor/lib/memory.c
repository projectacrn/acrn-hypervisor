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

void *memchr(const void *void_s, int c, size_t n)
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

/***********************************************************************
 *
 *   FUNCTION
 *
 *       memcpy_s
 *
 *   DESCRIPTION
 *
 *       Copies at most slen bytes from src address to dest address,
 *       up to dmax.
 *
 *   INPUTS
 *
 *       d                  pointer to Destination address
 *       dmax               maximum  length of dest
 *       s                  pointer to Source address
 *       slen               maximum number of bytes of src to copy
 *
 *   OUTPUTS
 *
 *       void *             pointer to destination address if successful,
 *                          or else return null.
 *
 ***********************************************************************/
void *memcpy_s(void *d, size_t dmax, const void *s, size_t slen_arg)
{
	uint8_t *dest8;
	uint8_t *src8;
	size_t slen = slen_arg;

	if ((slen == 0U) || (dmax == 0U) || (dmax < slen)) {
		ASSERT(false);
	}

	if ((((d) > (s)) && ((d) <= ((s + slen) - 1U)))
			|| (((d) < (s)) && ((s) <= ((d + dmax) - 1U)))) {
		ASSERT(false);
	}

	/* same memory block, no need to copy */
	if (d == s) {
		return d;
	}

	dest8 = (uint8_t *)d;
	src8 = (uint8_t *)s;

	/* small data block */
	if (slen < 8U) {
		while (slen != 0U) {
			*dest8 = *src8;
			dest8++;
			src8++;
			slen--;
		}

		return d;
	}

	/* make sure 8bytes-aligned for at least one addr. */
	if ((!mem_aligned_check((uint64_t)src8, 8UL)) &&
			(!mem_aligned_check((uint64_t)dest8, 8UL))) {
		for (; (slen != 0U) && ((((uint64_t)src8) & 7UL) != 0UL);
				slen--) {
			*dest8 = *src8;
			dest8++;
			src8++;
		}
	}

	/* copy main data blocks, with rep prefix */
	if (slen > 8U) {
		uint32_t ecx;

		asm volatile ("cld; rep; movsq"
				: "=&c"(ecx), "=&D"(dest8), "=&S"(src8)
				: "0" (slen >> 3), "1" (dest8), "2" (src8)
				: "memory");

		slen = slen & 0x7U;
	}

	/* tail bytes */
	while (slen != 0U) {
		*dest8 = *src8;
		dest8++;
		src8++;
		slen--;
	}

	return d;
}

void *memset(void *base, uint8_t v, size_t n)
{
	uint8_t *dest_p;
	size_t n_q;
	size_t count;
	void *ret;

	dest_p = (uint8_t *)base;

	if ((dest_p == NULL) || (n == 0U)) {
		ret = NULL;
	} else {
		/* do the few bytes to get uint64_t alignment */
		count = n;
		for (; (count != 0U) && (((uint64_t)dest_p & 7UL) != 0UL); count--) {
			*dest_p = v;
			dest_p++;
		}

		/* 64-bit mode */
		n_q = count >> 3U;
		asm volatile("cld ; rep ; stosq ; movl %3,%%ecx ; rep ; stosb"
					: "+c"(n_q), "+D"(dest_p)
					: "a" (v * 0x0101010101010101U),
					"r"((uint32_t)count  & 7U));
		ret = (void *)dest_p;
        }

	return ret;
}
