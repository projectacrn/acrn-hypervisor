/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <cpu.h>
#include <asm/lib/bits.h>


static volatile uint64_t pcpu_active_bitmap = 0UL;

/*
 * @post return <= MAX_PCPU_NUM
 */
uint16_t get_pcpu_nums(void)
{
	return arch_get_pcpu_num();
}

bool is_pcpu_active(uint16_t pcpu_id)
{
	return bitmap_test(pcpu_id, &pcpu_active_bitmap);
}

void set_pcpu_active(uint16_t pcpu_id)
{
	bitmap_set_lock(pcpu_id, &pcpu_active_bitmap);
}

void clear_pcpu_active(uint16_t pcpu_id)
{

	bitmap_clear_lock(pcpu_id, &pcpu_active_bitmap);
}

bool check_pcpus_active(uint64_t mask)
{
	return ((pcpu_active_bitmap & mask) == mask);
}

bool check_pcpus_inactive(uint64_t mask)
{
	return ((pcpu_active_bitmap & mask) != 0UL);
}

uint64_t get_active_pcpu_bitmap(void)
{
	return pcpu_active_bitmap;
}
