/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ACPI_H
#define ACPI_H

#include <vm_configurations.h>

#define ACPI_OEM_ID_SIZE           6

#define RSDP_CHECKSUM_LENGTH       20
#define ACPI_NAME_SIZE             4U
#define ACPI_MADT_TYPE_LOCAL_APIC  0U
#define ACPI_MADT_TYPE_IOAPIC  1U
#define ACPI_MADT_ENABLED          1U
#define ACPI_OEM_TABLE_ID_SIZE     8


/* FACP field offsets */
#define OFFSET_FACS_ADDR	36U
#define OFFSET_RESET_REGISTER	116U
#define OFFSET_RESET_VALUE	128U
#define OFFSET_FACS_X_ADDR	132U
#define OFFSET_PM1A_EVT         148U
#define OFFSET_PM1A_CNT         172U

/* FACS field offsets */
#define OFFSET_FACS_SIGNATURE	0U
#define OFFSET_FACS_LENGTH	4U
#define OFFSET_WAKE_VECTOR_32	12U
#define OFFSET_WAKE_VECTOR_64	24U

#define ACPI_SIG_FADT             "FACP" /* Fixed ACPI Description Table */
#define ACPI_SIG_FACS              0x53434146U /* "FACS" */
#define ACPI_SIG_RSDP             "RSD PTR " /* Root System Description Ptr */
#define ACPI_SIG_MADT             "APIC" /* Multiple APIC Description Table */
#define ACPI_SIG_DMAR             "DMAR"


struct packed_gas {
	uint8_t 	space_id;
	uint8_t 	bit_width;
	uint8_t 	bit_offset;
	uint8_t 	access_size;
	uint64_t	address;
} __packed;

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
} __packed;

struct acpi_table_rsdp {
	/* ACPI signature, contains "RSD PTR " */
	char                    signature[8];
	/* ACPI 1.0 checksum */
	uint8_t                 checksum;
	/* OEM identification */
	char                    oem_id[ACPI_OEM_ID_SIZE];
	/* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	uint8_t                 revision;
	/* 32-bit physical address of the RSDT */
	uint32_t                rsdt_physical_address;
	/* Table length in bytes, including header (ACPI 2.0+) */
	uint32_t                length;
	/* 64-bit physical address of the XSDT (ACPI 2.0+) */
	uint64_t                xsdt_physical_address;
	/* Checksum of entire table (ACPI 2.0+) */
	uint8_t                 extended_checksum;
	/* Reserved, must be zero */
	uint8_t                 reserved[3];
} __packed;

struct acpi_table_rsdt {
	/* Common ACPI table header */
	struct acpi_table_header   header;
	/* Array of pointers to ACPI tables */
	uint32_t                   table_offset_entry[1];
} __packed;

struct acpi_table_xsdt {
	/* Common ACPI table header */
	struct acpi_table_header    header;
	/* Array of pointers to ACPI tables */
	uint64_t                    table_offset_entry[1];
} __packed;

struct acpi_table_madt {
	/* Common ACPI table header */
	struct acpi_table_header     header;
	/* Physical address of local APIC */
	uint32_t                     address;
	uint32_t                     flags;
} __packed;

struct acpi_subtable_header {
	uint8_t                   type;
	uint8_t                   length;
} __packed;

struct acpi_madt_local_apic {
	struct acpi_subtable_header    header;
	/* ACPI processor id */
	uint8_t                        processor_id;
	/* Processor's local APIC id */
	uint8_t                        id;
	uint32_t                       lapic_flags;
} __packed;

struct acpi_madt_ioapic {
	struct acpi_subtable_header    header;
	/* IOAPIC id */
	uint8_t				id;
	uint8_t				rsvd;
	uint32_t			addr;
	uint32_t			gsi_base;
} __packed;

enum acpi_dmar_type {
	ACPI_DMAR_TYPE_HARDWARE_UNIT        = 0,
	ACPI_DMAR_TYPE_RESERVED_MEMORY      = 1,
	ACPI_DMAR_TYPE_ROOT_ATS             = 2,
	ACPI_DMAR_TYPE_HARDWARE_AFFINITY    = 3,
	ACPI_DMAR_TYPE_NAMESPACE            = 4,
	ACPI_DMAR_TYPE_RESERVED             = 5
};

struct acpi_table_dmar {
	/* Common ACPI table header */
	struct acpi_table_header  header;
	/* Host address Width */
	uint8_t                   width;
	uint8_t                   flags;
	uint8_t                   reserved[10];
} __packed;

/* DMAR subtable header */
struct acpi_dmar_header {
	uint16_t                  type;
	uint16_t                  length;
} __packed;

struct acpi_dmar_hardware_unit {
	struct acpi_dmar_header   header;
	uint8_t                   flags;
	uint8_t                   reserved;
	uint16_t                  segment;
	/* register base address */
	uint64_t                  address;
} __packed;

struct acpi_dmar_pci_path {
	uint8_t                   device;
	uint8_t                   function;
} __packed;

struct acpi_dmar_device_scope {
	uint8_t                   entry_type;
	uint8_t                   length;
	uint16_t                  reserved;
	uint8_t                   enumeration_id;
	uint8_t                   bus;
} __packed;


void *get_acpi_tbl(const char *signature);

struct ioapic_info;
uint16_t parse_madt(uint32_t lapic_id_array[CONFIG_MAX_PCPU_NUM]);
uint16_t parse_madt_ioapic(struct ioapic_info *ioapic_id_array);

#ifdef CONFIG_ACPI_PARSE_ENABLED
void acpi_fixup(void);
#endif

#endif /* !ACPI_H */
