/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PTCT_H
#define PTCT_H

#include <acpi.h>


struct ptct_entry_data_ptcm_binary
{
	uint64_t address;
	uint32_t size;
} __packed;


#endif /* PTCT_H */
