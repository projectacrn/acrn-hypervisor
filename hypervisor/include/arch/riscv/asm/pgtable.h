/*
 * Copyright (C) 2018-2024 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PGTABLE_H
#define PGTABLE_H

/* FIXME: Temporary RISC-V build workaround
 * This file provides hva2hpa function stubs to satisfy existing
 * code dependencies. Remove this file and migrate to the common
 * pgtable.h implementation once the MMU module is properly
 * integrated.
 */
static inline uint64_t hva2hpa(const void *x)
{
	return (uint64_t)x;
}

#endif /* PGTABLE_H */