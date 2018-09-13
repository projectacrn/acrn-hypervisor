/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* This is a template for uninitialized host_acpi_info,
 * we should use a user space tool running on target to generate this file.
 */

#include <hypervisor.h>

struct acpi_info host_acpi_info = {
	.x86_family = 0U,
	.x86_model = 0U,
	.pm_s_state = {
		.pm1a_evt = {
			.space_id = SPACE_SYSTEM_IO,
			.bit_width = 0U,
			.bit_offset = 0U,
			.access_size = 0U,
			.address = 0UL
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
			.bit_width = 0U,
			.bit_offset = 0U,
			.access_size = 0U,
			.address = 0UL
		},
		.pm1b_cnt = {
			.space_id = SPACE_SYSTEM_IO,
			.bit_width = 0U,
			.bit_offset = 0U,
			.access_size = 0U,
			.address = 0UL
		},
		.s3_pkg = {
			.val_pm1a = 0U,
			.val_pm1b = 0U,
			.reserved = 0U
		},
		.s5_pkg = {
			.val_pm1a = 0U,
			.val_pm1b = 0U,
			.reserved = 0U
		},
		.wake_vector_32 = (uint32_t *)0UL,
		.wake_vector_64 = (uint64_t *)0UL
	}
};
