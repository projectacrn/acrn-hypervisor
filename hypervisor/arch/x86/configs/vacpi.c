/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <vacpi.h>

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void build_vrsdp(struct acrn_vm *vm)
{
	struct acpi_table_rsdp rsdp = {
		.signature = ACPI_SIG_RSDP,
		.oem_id = ACPI_OEM_ID,
		.revision = 0x2U,
		.length = ACPI_RSDP_XCHECKSUM_LENGTH,
		.xsdt_physical_address = VIRT_XSDT_ADDR,
	};

	rsdp.checksum = calculate_checksum8(&rsdp, ACPI_RSDP_CHECKSUM_LENGTH);
	rsdp.extended_checksum = calculate_checksum8(&rsdp, ACPI_RSDP_XCHECKSUM_LENGTH);
	/* Copy RSDP table to guest physical memory F segment */
	(void)copy_to_gpa(vm, &rsdp, VIRT_RSDP_ADDR, ACPI_RSDP_XCHECKSUM_LENGTH);
}
