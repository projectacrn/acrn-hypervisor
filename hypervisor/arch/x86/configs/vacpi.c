/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <mmu.h>
#include <ept.h>
#include <vacpi.h>

static uint64_t sos_rsdp_gpa;

uint64_t get_vrsdp_gpa(struct acrn_vm *vm)
{
	uint64_t gpa = 0UL;

	if (is_sos_vm(vm)) {
		gpa = sos_rsdp_gpa;
	} else if (is_prelaunched_vm(vm)) {
		gpa = VIRT_RSDP_ADDR;
	}
	return gpa;
}

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 */
void build_vrsdp(struct acrn_vm *vm)
{
	if (is_sos_vm(vm)) {
		/* Reserve a memory area from host e820 space to store SOS VM ACPI RSDP info.
		 * This should be done before create_sos_vm_e820() because the SOS e820
		 * need to be updated from host e820 table accordingly.
		 */
		uint64_t sos_rsdp_hpa = e820_alloc_memory(MEM_1K, MEM_2G * 2);

		stac();
		(void)memcpy_s(hpa2hva(sos_rsdp_hpa), sizeof(struct acpi_table_rsdp),
				get_rsdp(), sizeof(struct acpi_table_rsdp));
		clac();
		sos_rsdp_gpa = sos_vm_hpa2gpa(sos_rsdp_hpa);

	} else if (is_prelaunched_vm(vm)) {
		struct acpi_table_rsdp rsdp = {
			.signature = ACPI_SIG_RSDP,
			.oem_id = ACPI_OEM_ID,
			.revision = 0x2U,
			.length = ACPI_RSDP_XCHECKSUM_LENGTH,
			.xsdt_physical_address = VIRT_XSDT_ADDR,
		};

		rsdp.checksum = calculate_checksum8(&rsdp, ACPI_RSDP_CHECKSUM_LENGTH);
		rsdp.extended_checksum = calculate_checksum8(&rsdp, ACPI_RSDP_XCHECKSUM_LENGTH);
		/* Copy RSDP table to guest physical memory F segment
		 * This should be done after prepare_prelaunched_vm_memmap()
		 */
		(void)copy_to_gpa(vm, &rsdp, VIRT_RSDP_ADDR, ACPI_RSDP_XCHECKSUM_LENGTH);
	}
}
