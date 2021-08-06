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
	struct acpi_table_tpm2 *vtpm2 = NULL, *tpm2 = NULL;
	struct acrn_vm_config *config = get_vm_config(vm_id);
	uint8_t checksum, i;
	struct acrn_boot_info *abi = get_acrn_boot_info();
	struct acrn_mmiodev *dev = NULL;

	struct abi_module *mod = get_mod_by_tag(abi, config->acpi_config.acpi_mod_tag);
	if (mod != NULL) {
		vtpm2 = get_acpi_mod_entry(ACPI_SIG_TPM2, mod->start);
		tpm2 = get_acpi_tbl(ACPI_SIG_TPM2);

		if (config->pt_tpm2 && (vtpm2 != NULL) && (tpm2 != NULL)) {
			for (i = 0U; i < MAX_MMIO_DEV_NUM; i++) {
				if (strncmp(config->mmiodevs[i].name, "tpm2", 4) == 0) {
					dev = &config->mmiodevs[i];
					break;
				}
			}

			if (dev != NULL) {
				vtpm2->start_method = tpm2->start_method;
				memcpy_s(&vtpm2->start_method_spec_para, sizeof(tpm2->start_method_spec_para),
					 &tpm2->start_method_spec_para, sizeof(tpm2->start_method_spec_para));

				/* tpm2 has event log */
				if (tpm2->laml != 0U) {
					vtpm2->header.length = tpm2->header.length;
					vtpm2->header.revision = tpm2->header.revision;
					vtpm2->laml = tpm2->laml;
					vtpm2->lasa = dev->res[1].user_vm_pa;

					/* update log buffer length/HPA in vm_config */
					dev->res[1].size = tpm2->laml;
					dev->res[1].host_pa = tpm2->lasa;
				}

				/* update checksum */
				vtpm2->header.checksum = 0;
				checksum = calculate_checksum8(vtpm2, sizeof(struct acpi_table_tpm2));
				vtpm2->header.checksum = checksum;
			} else {
				pr_err("%s, no TPM2 in acrn_vm_config", __func__);
			}
		}
	}
}

void security_vm_fixup(uint16_t vm_id)
{
	struct acrn_vm_config *vm_config = get_vm_config(vm_id);

	if ((vm_config->guest_flags & GUEST_FLAG_TPM2_FIXUP) != 0UL) {
		stac();
		tpm2_fixup(vm_id);
		clac();
	}
}

