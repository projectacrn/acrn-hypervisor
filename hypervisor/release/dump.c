/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void dump_intr_excp_frame(__unused const struct intr_excp_ctx *ctx) {}
void dump_exception(__unused struct intr_excp_ctx *ctx, __unused uint16_t pcpu_id) {}
