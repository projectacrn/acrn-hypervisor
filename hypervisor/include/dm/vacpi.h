/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/************************************************************************
 *
 *   FILE NAME
 *
 *       vacpi.h
 *
 *   DESCRIPTION
 *
 *       This file defines API and extern variable for virtual ACPI
 *
 ************************************************************************/
/**********************************/
/* EXTERNAL VARIABLES             */
/**********************************/
#ifndef VACPI_H
#define VACPI_H

#include <acpi.h>

/*
 *
 * Create the minimal set of ACPI tables required to boot pre-launched VM
 *
 *  The tables are placed in the guest's ROM area just below 1MB physical,
 * above the MPTable.
 *
 *  Layout
 *  ------
 *   RSDP  ->   0xf2400    (36 bytes fixed)
 *     XSDT  ->   0xf2480    (36 bytes + 8*7 table addrs, 4 used)
 *       FADT  ->   0xf2500  (244 bytes fixed for ACPI 2.0)
 *         DSDT -> 0xf2600      (36 bytes fixed for an empty DSDT)
 *       MCFG  ->   0xf2700  (36 bytes fixed + 8 bytes reserved + 1 * 16 bytes)
 *       MADT  ->   0xf2740  (depends on #CPUs)
 */
#define ACPI_BASE         0xf2400U

#define ACPI_RSDP_ADDR    (ACPI_BASE + 0x0U)
#define ACPI_XSDT_ADDR    (ACPI_BASE + 0x080U)
#define ACPI_FADT_ADDR    (ACPI_BASE + 0x100U)
#define ACPI_DSDT_ADDR    (ACPI_BASE + 0x200U)
#define ACPI_MCFG_ADDR    (ACPI_BASE + 0x300U)
#define ACPI_MADT_ADDR    (ACPI_BASE + 0x340U)
#define ACPI_TPM2_ADDR    (ACPI_BASE + 0x1000U)

#define ACPI_OEM_ID           "ACRN  "
#define ACPI_ASL_COMPILER_ID  "INTL"
#define ACPI_ASL_COMPILER_VERSION  0x20190802U

/* virtual PCI MMCFG address base for pre/post-launched VM. */
#define VIRT_PCI_MMCFG_BASE	0xE0000000UL

struct acrn_vm;
struct acpi_table_info {
	struct acpi_table_rsdp rsdp;
	struct acpi_table_xsdt xsdt;

	struct {
		struct acpi_table_fadt fadt;
		struct acpi_table_header dsdt;	/* an empty DSDT */
		struct acpi_table_mcfg mcfg;
		struct acpi_mcfg_allocation mcfg_entry;	/* mcfg_entry msut be declared fellowing mcfg */
		struct acpi_table_madt madt;
		struct acpi_madt_ioapic ioapic_struct;
		struct acpi_madt_local_apic_nmi lapic_nmi;
		struct acpi_madt_local_apic lapic_array[MAX_PCPU_NUM];
	} __packed;
};

void build_vacpi(struct acrn_vm *vm);

#endif /* VACPI_H */
