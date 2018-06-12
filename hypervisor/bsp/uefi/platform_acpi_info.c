/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template for uninitialized host_acpi_info,
 * we should use a user space tool running on target to generate this file.
 */

#include <hypervisor.h>

const struct acpi_info host_acpi_info = {
	-1,				/* x86 family */
	-1,				/* x86 model */
	{
		{SPACE_SYSTEM_IO, 0, 0, 0, 0},	/* PM1a EVT */
		{SPACE_SYSTEM_IO, 0, 0, 0, 0},	/* PM1b EVT */
		{SPACE_SYSTEM_IO, 0, 0, 0, 0},	/* PM1a CNT */
		{SPACE_SYSTEM_IO, 0, 0, 0, 0},	/* PM1b CNT */
		{0,	0,	0},		/* _S3 Package */
		{0,	0,	0},		/* _S5 Package */
		(uint32_t *)0,			/* Wake Vector 32 */
		(uint64_t *)0			/* Wake Vector 64 */
	}
};
