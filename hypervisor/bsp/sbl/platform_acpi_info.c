/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

const struct acpi_info host_acpi_info = {
	.x86_family = 6U,
	.x86_model = 0x5CU,			/* ApolloLake */
	.pm_s_state = {
		.pm1a_evt = {
			.space_id = SPACE_SYSTEM_IO,
			.bit_width = 0x20U,
			.bit_offset = 0U,
			.access_size = 3U,
			.address = 0x400UL
		},
		.pm1b_evt = {
			.space_id = SPACE_SYSTEM_IO,
			.bit_width = 0U,
			.bit_offset = 0U,
			.access_size = 0U,
			.address = 0UL
		},
		.pm1a_cnt = {
			.space_id = SPACE_SYSTEM_IO,
			.bit_width = 0x10U,
			.bit_offset = 0U,
			.access_size = 2U,
			.address = 0x404UL
		},
		.pm1b_cnt = {
			.space_id = SPACE_SYSTEM_IO,
			.bit_width = 0U,
			.bit_offset = 0U,
			.access_size = 0U,
			.address = 0UL
		},
		.s3_pkg = {
			.val_pm1a = 0x05U,
			.val_pm1b = 0U,
			.reserved = 0U
		},
		.s5_pkg = {
			.val_pm1a = 0x07U,
			.val_pm1b = 0U,
			.reserved = 0U
		},
		.wake_vector_32 = (uint32_t *)0x7A86BC9CUL,
		.wake_vector_64 = (uint64_t *)0x7A86BCA8UL
	}
};
