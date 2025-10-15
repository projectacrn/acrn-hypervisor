/*
 * Copyright (C) 2023-2025 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pgtable.h>

uint64_t arch_pgtl_large(uint64_t pgtle)
{
	return (pgtle & PAGE_V) && ((pgtle & PAGE_TYPE_MASK) != PAGE_TYPE_TABLE);
}

uint64_t arch_pgtl_page_paddr(uint64_t pgtle)
{
	uint64_t base = (pgtle & PTE_PFN_MASK) >> PAGE_BASE_OFFSET;
	return ((base << PTE_SHIFT) & PAGE_PFN_MASK);
}
