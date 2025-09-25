/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/irq.h>

struct intr_excp_ctx;

void dump_intr_excp_frame(__unused const struct intr_excp_ctx *ctx) {}
void dump_exception(__unused struct intr_excp_ctx *ctx, __unused uint16_t pcpu_id) {}
void asm_assert(__unused int32_t line, __unused const char *file, __unused const char *txt) {}
