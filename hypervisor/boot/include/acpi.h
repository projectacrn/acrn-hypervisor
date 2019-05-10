/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

#include <vm_configurations.h>

struct acpi_table_header {
	/* ASCII table signature */
	char                    signature[4];
	/* Length of table in bytes, including this header */
	uint32_t                length;
	/* ACPI Specification minor version number */
	uint8_t                 revision;
	/* To make sum of entire table == 0 */
	uint8_t                 checksum;
	/* ASCII OEM identification */
	char                    oem_id[6];
	/* ASCII OEM table identification */
	char                    oem_table_id[8];
	/* OEM revision number */
	uint32_t                oem_revision;
	/* ASCII ASL compiler vendor ID */
	char                    asl_compiler_id[4];
	/* ASL compiler version */
	uint32_t                asl_compiler_revision;
};

void *get_acpi_tbl(const char *signature);

struct ioapic_info;
uint16_t parse_madt(uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM]);
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array);

#ifndef CONFIG_CONSTANT_ACPI
void acpi_fixup(void);
#endif

#endif /* !ACPI_H */
