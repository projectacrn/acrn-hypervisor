/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <per_cpu.h>
#include <vacpi.h>
#include <pgtable.h>

/* ACPI tables for pre-launched VM and SOS */
static struct acpi_table_info acpi_table_template[CONFIG_MAX_VM_NUM] = {
	[0U ... (CONFIG_MAX_VM_NUM - 1U)] = {
		.rsdp = {
			.signature = ACPI_SIG_RSDP,
			.oem_id = ACPI_OEM_ID,
			.revision = 0x2U,
			.length = ACPI_RSDP_XCHECKSUM_LENGTH,
			.xsdt_physical_address = ACPI_XSDT_ADDR,
		},
		.xsdt = {
			/* Currently XSDT table only pointers to 1 ACPI table entry (MADT) */
			.header.length = sizeof(struct acpi_table_header) + sizeof(uint64_t),

			.header.revision = 0x1U,
			.header.oem_revision = 0x1U,
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.header.signature = ACPI_SIG_XSDT,
			.header.oem_id = ACPI_OEM_ID,
			.header.oem_table_id = "ACRNXSDT",
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID,

			.table_offset_entry[0] = ACPI_MADT_ADDR,
		},
		.madt = {
			.header.revision = 0x3U,
			.header.oem_revision = 0x1U,
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.header.signature = ACPI_SIG_MADT,
			.header.oem_id = ACPI_OEM_ID,
			.header.oem_table_id = "ACRNMADT",
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID,

			.address = 0xFEE00000U, /* Local APIC Address */
			.flags = 0x1U, /* PC-AT Compatibility=1 */
		},
		.lapic_nmi = {
			.header.type = ACPI_MADT_TYPE_LOCAL_APIC_NMI,
			.header.length = sizeof(struct acpi_madt_local_apic_nmi),
			.processor_id = 0xFFU,
			.flags = 0x5U,
			.lint = 0x1U,
		},
		.lapic_array = {
			[0U ... (CONFIG_MAX_PCPU_NUM - 1U)] = {
				.header.type = ACPI_MADT_TYPE_LOCAL_APIC,
				.header.length = sizeof(struct acpi_madt_local_apic),
				.lapic_flags = 0x1U, /* Processor Enabled=1, Runtime Online Capable=0 */
			}
		},
	}
};

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void build_vacpi(struct acrn_vm *vm)
{
	struct acpi_table_rsdp *rsdp;
	struct acpi_table_xsdt *xsdt;
	struct acpi_table_madt *madt;
	struct acpi_madt_local_apic *lapic;
	uint16_t i;

	rsdp = &acpi_table_template[vm->vm_id].rsdp;
	rsdp->checksum = calculate_checksum8(rsdp, ACPI_RSDP_CHECKSUM_LENGTH);
	rsdp->extended_checksum = calculate_checksum8(rsdp, ACPI_RSDP_XCHECKSUM_LENGTH);
	/* Copy RSDP table to guest physical memory */
	(void)copy_to_gpa(vm, rsdp, ACPI_RSDP_ADDR, ACPI_RSDP_XCHECKSUM_LENGTH);

	xsdt = &acpi_table_template[vm->vm_id].xsdt;
	xsdt->header.checksum = calculate_checksum8(xsdt, xsdt->header.length);
	/* Copy XSDT table to guest physical memory */
	(void)copy_to_gpa(vm, xsdt, ACPI_XSDT_ADDR, xsdt->header.length);

	/* Fix up MADT LAPIC subtables */
	for (i = 0U; i < vm->hw.created_vcpus; i++) {
		lapic = &acpi_table_template[vm->vm_id].lapic_array[i];
		lapic->processor_id = (uint8_t)i;
		lapic->id = (uint8_t)i;
	}

	madt = &acpi_table_template[vm->vm_id].madt;
	madt->header.length = sizeof(struct acpi_table_madt)
		+ sizeof(struct acpi_madt_local_apic_nmi)
		+ (sizeof(struct acpi_madt_local_apic) * (size_t)vm->hw.created_vcpus);
	madt->header.checksum = calculate_checksum8(madt, madt->header.length);

	/* Copy MADT table and its subtables to guest physical memory */
	(void)copy_to_gpa(vm, madt, ACPI_MADT_ADDR, madt->header.length);
}
