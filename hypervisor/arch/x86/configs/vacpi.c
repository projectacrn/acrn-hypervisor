/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <per_cpu.h>
#include <vacpi.h>
#include <pgtable.h>
#include <platform_acpi_info.h>

#define ACPI_TABLE_HEADER(SIGNATURE, LENGTH, REVISION, OEM_ID,			\
	OEM_TABLE_ID, OEM_REVISION, ASL_COMPILER_ID, ASL_COMPILER_REVISION) 	\
	{									\
		.signature = (SIGNATURE),					\
		.length = (LENGTH),						\
		.revision = (REVISION),						\
		.oem_id = (OEM_ID),						\
		.oem_table_id = (OEM_TABLE_ID),					\
		.oem_revision = (OEM_REVISION),					\
		.asl_compiler_id = (ASL_COMPILER_ID),				\
		.asl_compiler_revision = (ASL_COMPILER_REVISION),		\
	}

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
			.header = ACPI_TABLE_HEADER(ACPI_SIG_XSDT, 0U, 0x1U, ACPI_OEM_ID,
					"ACRNXSDT", 0x1U, ACPI_ASL_COMPILER_ID, ACPI_ASL_COMPILER_VERSION),

			.table_offset_entry[0] = ACPI_MADT_ADDR,
		},
		.fadt = {
			.header = ACPI_TABLE_HEADER(ACPI_SIG_FADT, 0xF4U, 0x3U, ACPI_OEM_ID,
					"ACRNFADT", 0x1U, ACPI_ASL_COMPILER_ID, ACPI_ASL_COMPILER_VERSION),

			.dsdt = ACPI_DSDT_ADDR,

			.pm1a_event_block = PM1A_EVT_ADDRESS,
			.pm1a_control_block = PM1A_CNT_ADDRESS,
			.pm1_event_length = 0x4U,
			.pm1_control_length = 0x02U,

			.flags = 0x00001125U,	/* HEADLESS | TMR_VAL_EXT | SLP_BUTTON | PROC_C1 | WBINVD */
		},
		.dsdt = ACPI_TABLE_HEADER(ACPI_SIG_DSDT, sizeof(struct acpi_table_header), 0x3U, ACPI_OEM_ID,
					"ACRNDSDT", 0x1U, ACPI_ASL_COMPILER_ID, ACPI_ASL_COMPILER_VERSION),
		.mcfg = {
			.header = ACPI_TABLE_HEADER(ACPI_SIG_MCFG, 0U, 0x3U, ACPI_OEM_ID,
					"ACRNMCFG", 0x1U, ACPI_ASL_COMPILER_ID, ACPI_ASL_COMPILER_VERSION),
		},
		.mcfg_entry = {
			.address = VIRT_PCI_MMCFG_BASE,
			.pci_segment = 0U,
			.start_bus_number = 0x0U,
			.end_bus_number = 0xFFU,
		},
		.madt = {
			.header = ACPI_TABLE_HEADER(ACPI_SIG_MADT, 0U, 0x3U, ACPI_OEM_ID,
					"ACRNMADT", 0x1U, ACPI_ASL_COMPILER_ID, ACPI_ASL_COMPILER_VERSION),

			.address = 0xFEE00000U, /* Local APIC Address */
			.flags = 0x1U, /* PC-AT Compatibility=1 */
		},
		.ioapic_struct = {
			.header.type = ACPI_MADT_TYPE_IOAPIC,
			.header.length = sizeof(struct acpi_madt_ioapic),
			.id = 0x1U,
			.addr = VIOAPIC_BASE,
		},
		.lapic_nmi = {
			.header.type = ACPI_MADT_TYPE_LOCAL_APIC_NMI,
			.header.length = sizeof(struct acpi_madt_local_apic_nmi),
			.processor_id = 0xFFU,
			.flags = 0x5U,
			.lint = 0x1U,
		},
		.lapic_array = {
			[0U ... (MAX_PCPU_NUM - 1U)] = {
				.header.type = ACPI_MADT_TYPE_LOCAL_APIC,
				.header.length = sizeof(struct acpi_madt_local_apic),
				.lapic_flags = 0x1U, /* Processor Enabled=1, Runtime Online Capable=0 */
			}
		},
	}
};

#ifdef VM0_PASSTHROUGH_TPM
static struct acpi_table_tpm2 tpm2 = {
	.header = ACPI_TABLE_HEADER(ACPI_SIG_TPM2, sizeof(struct acpi_table_tpm2), 0x3U, ACPI_OEM_ID,
			"ACRNTPM2", 0x1U, ACPI_ASL_COMPILER_ID, ACPI_ASL_COMPILER_VERSION),
	.control_address = 0xFED40040UL,
	.start_method = 0x7U,	/* Uses the Command Response Buffer Interface */
};

static uint8_t dsdt_data[45U] = {
		// [.5TPM_
		0x5B, 0x82, 0x35, 0x54, 0x50, 0x4D, 0x5F,
		// ._HID.MSFT0101.
		0x08, 0x5F, 0x48, 0x49, 0x44, 0x0D, 0x4D, 0x53, 0x46, 0x54, 0x30, 0x31, 0x30, 0x31, 0x00,
		// ._CRS
		0x08, 0x5F, 0x43, 0x52, 0x53,
		0x11, 0x11, 0x0A, 0x0E, 0x86, 0x09, 0x00, 0x01, 0x00, 0x00, 0xD4, 0xFE, 0x00, 0x50, 0x00, 0x00,
		0x79, 0x00 };
#else
static struct acpi_table_tpm2 tpm2;
static uint8_t dsdt_data[0U];
#endif

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (vm->min_mem_addr <= ACPI_XSDT_ADDR) && (ACPI_XSDT_ADDR < vm->max_mem_addr)
 */
void build_vacpi(struct acrn_vm *vm)
{
	struct acpi_table_rsdp *rsdp;
	struct acpi_table_xsdt *xsdt;
	struct acpi_table_fadt *fadp;
	struct acpi_table_header *dsdt;
	struct acpi_table_mcfg *mcfg;
	struct acpi_table_madt *madt;
	struct acpi_madt_local_apic *lapic;
	uint16_t i, table_entry = 3U;
	bool pt_tpm2_acpitable = get_vm_config(vm->vm_id)->pt_tpm2;

	rsdp = &acpi_table_template[vm->vm_id].rsdp;
	rsdp->checksum = calculate_checksum8(rsdp, ACPI_RSDP_CHECKSUM_LENGTH);
	rsdp->extended_checksum = calculate_checksum8(rsdp, ACPI_RSDP_XCHECKSUM_LENGTH);
	/* Copy RSDP table to guest physical memory */
	(void)copy_to_gpa(vm, rsdp, ACPI_RSDP_ADDR, ACPI_RSDP_XCHECKSUM_LENGTH);

	xsdt = &acpi_table_template[vm->vm_id].xsdt;
	/* Copy XSDT table to guest physical memory */
	(void)copy_to_gpa(vm, xsdt, ACPI_XSDT_ADDR, sizeof(struct acpi_table_header));
	xsdt = (struct acpi_table_xsdt *)gpa2hva(vm, ACPI_XSDT_ADDR);
	stac();
	xsdt->table_offset_entry[0] = ACPI_FADT_ADDR;
	xsdt->table_offset_entry[1] = ACPI_MCFG_ADDR;
	xsdt->table_offset_entry[2] = ACPI_MADT_ADDR;
	if (pt_tpm2_acpitable) {
		xsdt->table_offset_entry[table_entry++] = ACPI_TPM2_ADDR;
	}
	/* Currently XSDT table only pointers to 3 ACPI table entry (FADT/MCFG/MADT) */
	xsdt->header.length = sizeof(struct acpi_table_header) + (table_entry * sizeof(uint64_t));
	xsdt->header.checksum = calculate_checksum8(xsdt, xsdt->header.length);
	clac();

	fadp = &acpi_table_template[vm->vm_id].fadt;
	fadp->header.checksum = calculate_checksum8(fadp, fadp->header.length);

	/* Copy FADT table to guest physical memory */
	(void)copy_to_gpa(vm, fadp, ACPI_FADT_ADDR, fadp->header.length);

	dsdt = &acpi_table_template[vm->vm_id].dsdt;

	/* Copy DSDT table and its subtables to guest physical memory */
	(void)copy_to_gpa(vm, dsdt, ACPI_DSDT_ADDR, dsdt->length);
	if (pt_tpm2_acpitable) {
		(void)copy_to_gpa(vm, dsdt_data, ACPI_DSDT_ADDR + dsdt->length, sizeof(dsdt_data));
	}
	dsdt = (struct acpi_table_header *)gpa2hva(vm, ACPI_DSDT_ADDR);
	stac();
	if (pt_tpm2_acpitable) {
		dsdt->length += sizeof(dsdt_data);
	}
	dsdt->checksum = calculate_checksum8(dsdt, dsdt->length);
	clac();

       mcfg = &acpi_table_template[vm->vm_id].mcfg;
       mcfg->header.length = sizeof(struct acpi_table_mcfg)
               + (1U * sizeof(struct acpi_mcfg_allocation));   /* We only support one mcfg allocation structure */
       mcfg->header.checksum = calculate_checksum8(mcfg, mcfg->header.length);

       /* Copy MCFG table and its subtables to guest physical memory */
       (void)copy_to_gpa(vm, mcfg, ACPI_MCFG_ADDR, mcfg->header.length);

	/* Fix up MADT LAPIC subtables */
	for (i = 0U; i < vm->hw.created_vcpus; i++) {
		lapic = &acpi_table_template[vm->vm_id].lapic_array[i];
		lapic->processor_id = (uint8_t)i;
		lapic->id = (uint8_t)i;
	}

	madt = &acpi_table_template[vm->vm_id].madt;
	madt->header.length = sizeof(struct acpi_table_madt) + sizeof(struct acpi_madt_ioapic)
		+ sizeof(struct acpi_madt_local_apic_nmi)
		+ (sizeof(struct acpi_madt_local_apic) * (size_t)vm->hw.created_vcpus);
	madt->header.checksum = calculate_checksum8(madt, madt->header.length);

	/* Copy MADT table and its subtables to guest physical memory */
	(void)copy_to_gpa(vm, madt, ACPI_MADT_ADDR, madt->header.length);

	if (pt_tpm2_acpitable) {
		tpm2.header.checksum = calculate_checksum8(&tpm2, tpm2.header.length);
		(void)copy_to_gpa(vm, &tpm2, ACPI_TPM2_ADDR, tpm2.header.length);
	}
}
