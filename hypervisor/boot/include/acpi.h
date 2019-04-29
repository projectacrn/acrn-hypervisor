/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

struct ioapic_info;
uint16_t parse_madt(uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM]);
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array);

#ifndef CONFIG_CONSTANT_ACPI
void acpi_fixup(void);
#endif

#endif /* !ACPI_H */
