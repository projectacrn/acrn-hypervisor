/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RISCV_MMU_H
#define RISCV_MMU_H

#include <types.h>
#include <asm/page.h>
#include <asm/pgtable.h>

static inline void switch_satp(uint64_t satp)
{
	asm volatile (
		"csrw satp, %0\n\t" \
		"sfence.vma"
		:: "r"(satp)
		: "memory"
	);
}
void init_paging(void);

#endif /* RISCV_MMU_H */
