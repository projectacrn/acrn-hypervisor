/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <logmsg.h>

void __stack_chk_fail(void);

void __stack_chk_fail(void)
{
	ASSERT(false, "stack check fails in HV\n");
}
