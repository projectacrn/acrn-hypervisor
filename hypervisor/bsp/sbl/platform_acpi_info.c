/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

const struct acpi_info host_acpi_info = {
	6,				/* x86 family: 6 */
	0x5C,				/* x86 model: 0x5C, ApolloLake */
	{
		{SPACE_SYSTEM_IO, 20, 0, 3, 0x400},	/* PM1a EVT */
		{SPACE_SYSTEM_IO,  0, 0, 0,     0},	/* PM1b EVT */
		{SPACE_SYSTEM_IO, 10, 0, 2, 0x404},	/* PM1a CNT */
		{SPACE_SYSTEM_IO,  0, 0, 0,     0},	/* PM1b CNT */
		{0x05,	0,	0},			/* _S3 Package */
		{0x07,	0,	0},			/* _S5 Package */
		(uint32_t *)0x7AEDCEFC,			/* Wake Vector 32 */
		(uint64_t *)0x7AEDCF08			/* Wake Vector 64 */
	}
};
