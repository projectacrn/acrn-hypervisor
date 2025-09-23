/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MMU_H
#define MMU_H


#include <asm/mmu.h>

void set_paging_supervisor(uint64_t base, uint64_t size);

#endif /* MMU_H */
