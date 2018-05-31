/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void __stack_chk_fail(void)
{
	ASSERT(0, "stack check fails in HV\n");
}
