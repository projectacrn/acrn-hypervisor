/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PAGE_H
#define PAGE_H

#include <spinlock.h>
#include <board_info.h>
#include <asm/mm_common.h>

/**
 * @defgroup hwmgmt_page hwmgmt.page
 * @ingroup hwmgmt
 * @brief Support the basic paging mechanism.
 *
 * This module mainly provides the interfaces to manipulate the paging structures.
 * These operations are commonly used by:
 * 1. hypervisor's MMU (Memory Management Unit) to manage the host page tables;
 * 2. EPT to manage the extended page tables for guest.
 * It also provides the interfaces to conduct the address translation between Host Physical Address and Host Virtual
 * Address.
 *
 * @{
 */

/**
 * @file
 * @brief All APIs to support page management.
 *
 * This file defines macros, structures and function declarations for managing memory pages.
 */

#define PAGE_SHIFT	12U
#define PAGE_SIZE	(1U << PAGE_SHIFT)
#define PAGE_MASK	0xFFFFFFFFFFFFF000UL

#define MAX_PHY_ADDRESS_SPACE	(1UL << MAXIMUM_PA_WIDTH)

/* size of the low MMIO address space: 2GB */
#define PLATFORM_LO_MMIO_SIZE	0x80000000UL

/* size of the high MMIO address space: 1GB */
#define PLATFORM_HI_MMIO_SIZE	0x40000000UL

/**
 * @brief Calculate the number of page map level-4(PML4) that is requested to control the memory region with the
 *        specified size.
 *
 * Page map level-4(PML4) table is the top-level table in the x86-64 paging hierarchy. Each entry in the PML4 table can
 * potentially map a 512 GiB region, with the entire PML4 table capable of addressing up to 256 TiB. So 1 PML4 table is
 * enough to control the entire physical address space.
 */
#define PML4_PAGE_NUM(size)	1UL
/**
 * @brief Calculate the number of page directory pointer tables(PDPT) that is requested to control the memory region
 *        with the specified size.
 *
 * A page directory pointer table(PDPT) can be referenced by a PML4E and each PML4E controls access to a 512-GByte
 * region. It is supposed to be called when hypervisor allocates the page-directory-pointer tables for hypervisor and
 * all VMs.
 */
#define PDPT_PAGE_NUM(size)	(((size) + PGTL3_SIZE - 1UL) >> PGTL3_SHIFT)
/**
 * @brief Calculate the number of page directories(PD) that is requested to control the memory region with the specified
 *        size.
 *
 * A page directory(PD) can be referenced by a PDPTE and each PDPTE controls access to a 1-GByte region. It is supposed
 * to be called when hypervisor allocates the page directories for hypervisor and all VMs.
 */
#define PD_PAGE_NUM(size)	(((size) + PGTL2_SIZE - 1UL) >> PGTL2_SHIFT)
/**
 * @brief Calculate the number of page tables(PT) that is requested to control the memory region with the specified
 *        size.
 *
 * A page table(PT) can be referenced by a PDE and each PDE controls access to a 2-MByte region. It is supposed to be
 * called when hypervisor allocates the page tables for hypervisor and all VMs.
 */
#define PT_PAGE_NUM(size)	(((size) + PGTL1_SIZE - 1UL) >> PGTL1_SHIFT)

/**
 * @brief Data structure to illustrate a 4-KByte memory region with an alignment of 4-KByte.
 *
 * This data structure is used to illustrate a 4-KByte memory region with an alignment of 4-KByte, calling it a 4-KByte
 * page. It can be used to support the memory management in hypervisor and the extended page-table mechanism for VMs. It
 * can also be used when hypervisor accesses the 4-KByte aligned memory region whose size is a multiple of 4-KByte.
 *
 * @consistency N/A
 * @alignment 4096
 *
 * @remark N/A
 */
struct page {
	uint8_t contents[PAGE_SIZE]; /**< A 4-KByte page in the memory. */
} __aligned(PAGE_SIZE);

#endif /* PAGE_H */

/**
 * @}
 */
