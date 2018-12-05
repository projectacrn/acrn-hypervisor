/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

uint16_t parse_madt(uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM]);

#endif /* !ACPI_H */
