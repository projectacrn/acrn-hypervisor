/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void    init_bsp(void)
{
#ifndef CONFIG_CONSTANT_ACPI
	acpi_fixup();
#endif
}
