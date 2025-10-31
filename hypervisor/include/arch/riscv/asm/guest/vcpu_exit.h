/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VCPU_EXIT_H
#define RISCV_VCPU_EXIT_H

#include <types.h>
#include <errno.h>

#ifndef ASSEMBLER

void dispatch_vcpu_trap();

#endif /* ASSEMBLER */

#endif /* RISCV_VCPU_EXIT_H */
