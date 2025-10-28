/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VCPU_H
#define RISCV_VCPU_H

#include <types.h>
#include <errno.h>

#ifndef ASSEMBLER

struct acrn_vcpu_arch {
} __aligned(PAGE_SIZE);

#endif /* ASSEMBLER */

#endif /* RISCV_VCPU_H */
