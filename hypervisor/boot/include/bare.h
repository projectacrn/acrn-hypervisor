/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BARE_BOOT_H
#define BARE_BOOT_H

struct bare_boot_option {
	uint64_t addr;
	uint64_t size;
	const char *tag;
};

int32_t init_bare_boot_info();

#endif /* BARE_BOOT_H */
