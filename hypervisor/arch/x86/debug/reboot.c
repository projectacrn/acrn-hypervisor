/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>
#include <reboot.h>

int warm_reboot(void)
{
	pio_write8(0x6, 0xcf9);
	return 0;
}
