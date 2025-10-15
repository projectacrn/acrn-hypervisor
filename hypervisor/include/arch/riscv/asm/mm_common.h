/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __RISCV_MM_COMMON_H__
#define __RISCV_MM_COMMON_H__
#include <asm/pgtable.h>

#define PGTL3_SHIFT VPN3_SHIFT
#define PGTL3_SIZE (1UL << PGTL3_SHIFT)
#define PGTL3_MASK (~(PGTL3_SIZE - 1UL))
#define PTRS_PER_PGTL3E PTRS_PER_VPN3
#define PGTL2_SHIFT VPN2_SHIFT
#define PGTL2_SIZE (1UL << PGTL2_SHIFT)
#define PGTL2_MASK (~(PGTL2_SIZE - 1UL))
#define PTRS_PER_PGTL2E PTRS_PER_VPN2
#define PGTL1_SHIFT VPN1_SHIFT
#define PGTL1_SIZE (1UL << PGTL1_SHIFT)
#define PGTL1_MASK (~(PGTL1_SIZE - 1UL))
#define PTRS_PER_PGTL1E PTRS_PER_VPN1
#define PGTL0_SHIFT PTE_SHIFT
#define PGTL0_SIZE (1UL << PGTL0_SHIFT)
#define PGTL0_MASK (~(PGTL0_SIZE - 1UL))
#define PTRS_PER_PGTL0E PTRS_PER_PTE
#define PAGE_PFN_OFFSET 10UL

#endif /*__RISCV_MM_COMMON_H__ */
