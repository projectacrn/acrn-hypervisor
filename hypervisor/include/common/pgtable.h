/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef COMMON_PGTABLE_H
#define COMMON_PGTABLE_H
#include <asm/page.h>

/**
 * @brief Translate a host physical address to a host virtual address.
 *
 * This function is used to translate a host physical address to a host virtual address. HPA is 1:1 mapping to HVA.
 *
 * It returns the host virtual address that corresponds to the given host physical address.
 *
 * @param[in] hpa The host physical address to be translated.
 *
 * @return The translated host virtual address
 *
 * @retval NULL if hpa == 0
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used after paging mode enabled.
 */
static inline void *hpa2hva(uint64_t hpa)
{
	return (void *)hpa;
}

/**
 * @brief Translate a host virtual address to a host physical address.
 *
 * This function is used to translate a host virtual address to a host physical address. HVA is 1:1 mapping to HPA.
 *
 * It returns the host physical address that corresponds to the given host virtual address.
 *
 * @param[in] va The host virtual address to be translated.
 *
 * @return The translated host physical address.
 *
 * @retval 0 if va == NULL
 *
 * @pre N/A
 *
 * @post N/A
 *
 * @remark This function is used after paging mode enabled.
 */
static inline uint64_t hva2hpa(const void *va)
{
	return (uint64_t)va;
}

static inline uint64_t round_page_up(uint64_t addr)
{
	return (((addr + (uint64_t)PAGE_SIZE) - 1UL) & PAGE_MASK);
}

static inline uint64_t round_page_down(uint64_t addr)
{
	return (addr & PAGE_MASK);
}
#endif /* COMMON_PGTABLE_H*/
