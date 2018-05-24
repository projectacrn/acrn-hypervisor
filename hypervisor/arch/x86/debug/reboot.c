/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>

int warm_reboot(void)
{
	io_write_byte(0x6, 0xcf9);
	return 0;
}
