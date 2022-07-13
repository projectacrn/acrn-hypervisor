/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* typedef size_t in types.h is conflicted with stdio.h, use below method as WR */
#define size_t new_size_t
#include <stdio.h>
#undef size_t
#include <util.h>

bool sanitize_vm_config(void);
