/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_SECURITY_H
#define RISCV_SECURITY_H

#ifdef STACK_PROTECTOR
#include <random.h>
#endif

#ifdef STACK_PROTECTOR

extern unsigned long __stack_chk_guard;

/*
 * Initialize the stack protector __stack_chk_guard.
 *
 * NOTE: this function changes the __stack_chk_guard,
 *  - It must only be called from functions that never return
 *  - It must always be inlined itself
 */
static inline __attribute__((__always_inline__)) void init_stack_canary(void)
{
	__stack_chk_guard = get_random_value();
}

#endif /* STACK_PROTECTOR */

#endif /* RISCV_SECURITY_H */
