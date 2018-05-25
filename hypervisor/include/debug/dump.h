/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef DUMP_H
#define DUMP_H

struct intr_excp_ctx;

#ifdef HV_DEBUG
#define CALL_TRACE_HIERARCHY_MAX    20
#define DUMP_STACK_SIZE 0x200

void dump_intr_excp_frame(struct intr_excp_ctx *ctx);
void dump_exception(struct intr_excp_ctx *ctx, uint32_t cpu_id);

#else
static inline void dump_intr_excp_frame(__unused struct intr_excp_ctx *ctx)
{
}

static inline void dump_exception(__unused struct intr_excp_ctx *ctx,
		__unused uint32_t cpu_id)
{
}

#endif

#endif /* DUMP_H */
