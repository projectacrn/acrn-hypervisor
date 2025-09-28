/*
 * Copyright (C) 2018-2024 Intel Corporation.
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

#endif /* RISCV_PGTABLE_H */
