/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <e820.h>

const struct e820_entry ve820_entry[E820_MAX_ENTRIES] = {
	{	/* 0 to mptable */
		.baseaddr = 0x0UL,
		.length   = 0xEFFFFUL,
		.type     = E820_TYPE_RAM
	},

	{	/* mptable 65536U */
		.baseaddr = 0xF0000UL,
		.length   = 0x10000UL,
		.type     = E820_TYPE_RESERVED
	},

	{	/* mptable to lowmem */
		.baseaddr = 0x100000UL,
		.length   = 0x7FF00000UL,
		.type     = E820_TYPE_RAM
	},

	{	/* lowmem to PCI hole */
		.baseaddr = 0x80000000UL,
		.length   = 0x40000000UL,
		.type     = E820_TYPE_RESERVED
	},

	{	/* PCI hole to 4G */
		.baseaddr = 0xe0000000UL,
		.length   = 0x20000000UL,
		.type     = E820_TYPE_RESERVED
	},
};
