/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_PRIV_H
#define ACPI_PRIV_H

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

void *get_dmar_table(void);
#endif /* !ACPI_PRIV_H */
