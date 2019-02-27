/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <e820.h>

const struct e820_entry ve820_entry[E820_MAX_ENTRIES] = {
	{	/* usable RAM under 1MB */
		.baseaddr = 0x0UL,
		.length   = 0xF0000UL,		/* 960KB */
		.type     = E820_TYPE_RAM
	},

	{	/* mptable */
		.baseaddr = 0xF0000UL,		/* 960KB */
		.length   = 0x10000UL,		/* 16KB */
		.type     = E820_TYPE_RESERVED
	},

	{	/* lowmem */
		.baseaddr = 0x200000UL,		/* 2MB */
		.length   = 0x7FE00000UL,	/* 2046MB */
		.type     = E820_TYPE_RAM
	},

	{	/* between lowmem and PCI hole */
		.baseaddr = 0x80000000UL,	/* 2GB */
		.length   = 0x40000000UL,	/* 1GB */
		.type     = E820_TYPE_RESERVED
	},

	{	/* between PCI hole and 4GB */
		.baseaddr = 0xe0000000UL,	/* 3.5GB */
		.length   = 0x20000000UL,	/* 512MB */
		.type     = E820_TYPE_RESERVED
	},
};
