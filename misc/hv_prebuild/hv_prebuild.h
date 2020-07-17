/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* typedef size_t in types.h is conflicted with stdio.h, use below method as WR */
#define size_t new_size_t
#include <stdio.h>
#undef size_t

bool uuid_is_equal(const uint8_t *uuid1, const uint8_t *uuid2);
bool sanitize_vm_config(void);
