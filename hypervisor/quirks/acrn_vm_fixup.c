/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <asm/vm_config.h>
#include <asm/guest/vm.h>
#include <vacpi.h>
#include <logmsg.h>

static void *get_acpi_mod_entry(const char *signature, void *acpi)
{
	struct acpi_table_xsdt *xsdt;
	uint32_t i, entry_cnt = 0U;
	struct acpi_table_header *header = NULL, *find = NULL;

	xsdt = acpi + VIRT_XSDT_ADDR - VIRT_ACPI_DATA_ADDR;
	entry_cnt = (xsdt->header.length - sizeof(xsdt->header)) / (sizeof(xsdt->table_offset_entry[0]));

	for (i = 0; i < entry_cnt; i++) {
		header = acpi + xsdt->table_offset_entry[i] - VIRT_ACPI_DATA_ADDR;
		if (strncmp(header->signature, signature, ACPI_NAME_SIZE) == 0) {
			find = header;
			break;
		}
	}

	return find;
}

static void tpm2_fixup(uint16_t vm_id)
{
	struct acpi_table_tpm2 *tpm2 = NULL, *native = NULL;
	struct acrn_vm_config *config = get_vm_config(vm_id);
	bool need_fix = false;
	uint8_t checksum;
	struct acrn_boot_info *abi = get_acrn_boot_info();
	struct abi_module *mod;

	mod = get_mod_by_tag(abi, config->acpi_config.acpi_mod_tag);
	if (mod != NULL) {
		tpm2 = get_acpi_mod_entry(ACPI_SIG_TPM2, mod->start);
		native = get_acpi_tbl(ACPI_SIG_TPM2);

		if (config->pt_tpm2) {
			if ((tpm2 != NULL) && (native != NULL)) {
				/* Native has different start method */
				need_fix = tpm2->start_method != native->start_method;

				/* Native has event log */
				if (native->header.length ==
				    sizeof(struct acpi_table_tpm2)) {
					need_fix |= tpm2->header.length == 0x34U;
					need_fix |= strncmp((char *)tpm2->start_method_spec_para,
							(char *)native->start_method_spec_para,
							sizeof(tpm2->start_method_spec_para)) != 0;
					need_fix |= tpm2->laml != native->laml;
					need_fix |= tpm2->lasa != native->lasa;
				}

				if (need_fix) {
					pr_err("%s tpm2 fix start method and event log field", __FUNCTION__);
					tpm2->start_method = native->start_method;
					tpm2->header.length = native->header.length;
					tpm2->header.revision = native->header.revision;
					memcpy_s(&tpm2->start_method_spec_para,
						sizeof(native->start_method_spec_para),
						&native->start_method_spec_para,
						sizeof(native->start_method_spec_para));
					tpm2->laml = native->laml;
					tpm2->lasa = config->mmiodevs[0].mmiores[1].base_gpa;

					tpm2->header.checksum = 0;
					checksum = calculate_checksum8(tpm2, sizeof(struct acpi_table_tpm2));
					tpm2->header.checksum = checksum;

					config->mmiodevs[0].mmiores[1].base_hpa = native->lasa;
					config->mmiodevs[0].mmiores[1].size = tpm2->laml;
				}
			} else {
				pr_err("VM or native can't find TPM2 ACPI table");
			}
		}
	}
}

void acrn_vm_fixup(uint16_t vm_id)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);

	if ((vm_config->load_order == PRE_LAUNCHED_VM)) {
		stac();
		tpm2_fixup(vm_id);
		clac();
	}
}

