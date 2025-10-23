/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/security.h>

#ifdef STACK_PROTECTOR
unsigned long __stack_chk_guard;
#endif
