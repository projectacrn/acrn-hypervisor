/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef RISCV_PGTABLE_H
#define RISCV_PGTABLE_H

#include <asm/page.h>

/* FIXME: Temporary RISC-V build workaround
 * This file provides hva2hpa[_early] and hpa2hva[_early] function stubs to
 * satisfy existing code dependencies. Remove this file and migrate to the
 * common pgtable.h implementation once the MMU module is properly integrated.
 */
static inline void *hpa2hva_early(uint64_t x)
{
	return (void *)x;
}

static inline uint64_t hva2hpa_early(void *x)
{
	return (uint64_t)x;
}

#define SATP_MODE_SV48			0x9000000000000000UL
#define SATP_PPN_MASK			0x00000FFFFFFFFFFFUL

#define PG_TABLE_SHIFT			9UL
#define PG_TABLE_ENTRIES		(_AC(1,U) << PG_TABLE_SHIFT)
#define PG_TABLE_ENTRY_MASK		(PG_TABLE_ENTRIES - 1)

#define  BIT0   			0x00000001UL
#define  BIT1   			0x00000002UL
#define  BIT2   			0x00000004UL
#define  BIT3   			0x00000008UL
#define  BIT4   			0x00000010UL
#define  BIT5   			0x00000020UL
#define  BIT6   			0x00000040UL
#define  BIT7   			0x00000080UL
#define  BIT8   			0x00000100UL
#define  BIT9   			0x00000200UL
#define  BIT10  			0x00000400UL
#define  BIT11  			0x00000800UL
#define  BIT12  			0x00001000UL
#define  BIT13  			0x00002000UL
#define  BIT14  			0x00004000UL
#define  BIT15  			0x00008000UL

#define PTE_ENTRY_COUNT			512UL
#define PTE_ADDR_MASK_BLOCK_ENTRY	(0xFFFFFFFFFULL << 10UL)

#define PAGE_ATTR_MASK			(((uint64_t)0x3UL) << 61UL)
#define PAGE_ATTR_PMA			(0x0UL)
#define PAGE_ATTR_UC			(((uint64_t)0x1UL) << 61UL)
#define PAGE_ATTR_IO			(((uint64_t)0x2UL) << 61UL)
#define PAGE_ATTR_RSV			(((uint64_t)0x3UL) << 61UL)

#define PAGE_CONF_MASK 			0xffUL
#define PAGE_V  			BIT0
#define PAGE_R  			BIT1
#define PAGE_W  			BIT2
#define PAGE_X  			BIT3
#define PAGE_U  			BIT4
#define PAGE_G  			BIT5
#define PAGE_A  			BIT6
#define PAGE_D  			BIT7

#define PAGE_TYPE_MASK			0xfUL
#define PAGE_TYPE_TABLE 		(0x0UL | PAGE_V)

#define PAGE_NO_RW  			(PAGE_V | PAGE_R | PAGE_W)
#define PAGE_RW_RW  			(PAGE_V | PAGE_U | PAGE_R | PAGE_W)
#define PAGE_NO_RO  			(PAGE_V | PAGE_R)
#define PAGE_RO_RO  			(PAGE_V | PAGE_U | PAGE_R)

#define PAGE_ATTRIBUTES_MASK  		(PAGE_CONF_MASK | PAGE_ATTR_MASK)

#define DEFINE_PAGE_TABLES(name, nr)					\
pgtable_t __aligned(PAGE_SIZE) name[PG_TABLE_ENTRIES * (nr)]

#define DEFINE_PAGE_TABLE(name) DEFINE_PAGE_TABLES(name, 1)

#define PTE_PFN_MASK   			0x3FFFFFFFFFFC00UL
#define PAGE_BASE_OFFSET 		10UL

/* for Sv48, vpn0 shift is 12 */
#define PTE_SHIFT			(PAGE_SHIFT)
#define PTRS_PER_PTE			(PG_TABLE_ENTRIES)

/* for Sv48, vpn1 shift is 21 */
#define VPN1_SHIFT			(PTE_SHIFT + PG_TABLE_SHIFT)
#define PTRS_PER_VPN1			(PG_TABLE_ENTRIES)

/* for Sv48, vpn2 shift is 30 */
#define VPN2_SHIFT			(VPN1_SHIFT + PG_TABLE_SHIFT)
#define PTRS_PER_VPN2			(PG_TABLE_ENTRIES)

/* for Sv48, vpn3 shift is 39 */
#define VPN3_SHIFT			(VPN2_SHIFT + PG_TABLE_SHIFT)
#define PTRS_PER_VPN3			(PG_TABLE_ENTRIES)

#define PAGE_PFN_MASK 			0x0000FFFFFFFFF000UL
#define PFN_MASK PTE_PFN_MASK

#define PPT_PFN_HIGH_MASK		0xFFFF000000000000UL
#define INVALID_HPA			(0x1UL << 52U)

#ifndef __ASSEMBLY__

#define IS_ALIGNED(val, align)	(((val) & ((align) - 1)) == 0)

#ifdef __ASSEMBLY__
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

struct page {
	uint8_t contents[PAGE_SIZE];
} __aligned(PAGE_SIZE);

typedef uint64_t pgtable_t;

/*
 * Memory Type
 */
#define MT_PMA		0x0UL
#define MT_UC		0x1UL
#define MT_IO		0x2UL
#define MT_RSV		0x3UL

#endif /* __ASSEMBLY__ */

#endif /* RISCV_PGTABLE_H */
