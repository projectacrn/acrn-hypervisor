/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _PAGE_MISC_H_
#define _PAGE_MISC_H_

#include <asm/page.h>
#include <pgtable.h>
#include <mmu.h>

void *pgtable_create_trusty_root(const struct pgtable *table,
	void *nworld_pml4_page, uint64_t prot_table_present, uint64_t prot_clr);
void early_pgtable_map_uart(uint64_t addr);

#endif /* _PAGE_MISC_H_ */
