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
#include <util.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/guest/ept.h>
#include <acrn_common.h>
#include <quirks/smbios.h>
#include <boot.h>

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

	if ((vm_config->guest_flags & GUEST_FLAG_SECURITY_VM) != 0UL) {
		stac();
		tpm2_fixup(vm_id);
		clac();
	}
}

/* Below are code for SMBIOS passthrough */

/* The region after the first 64kb is currently not used. On some platforms,
 * there might be an TPM2 event log region starting from VIRT_ACPI_NVS_ADDR + 0xB0000.
 * This table is usually a few KB in size so we have plenty of room here.
 */
#define VIRT_SMBIOS_TABLE_ADDR (VIRT_ACPI_NVS_ADDR + 64 * MEM_1K)
/* We hardcode the eps addr to 0xf1000. The ACPI RSDP addr is hardcoded to 0xf2400
 * so we're safe here. This header is at most 31 bytes.
 */
#define VIRT_SMBIOS_EPS_ADDR    0xf1000UL
#define SMBIOS_EPS_SEARCH_START 0xf0000UL
#define SMBIOS_EPS_SEARCH_END   0xfffffUL

/* For subsequent code, "smbios2" will be specifically refering to 32bit SMBIOS (major version 2),
 * and "smbios3" will refer to 64-bit SMBIOS (major version 3). "smbios" will be a generic
 * reference to SMBIOS structure.
 */
struct smbios_info {
    union {
        struct smbios2_entry_point eps2;
        struct smbios3_entry_point eps3;
    } smbios_eps;
    size_t smbios_eps_size;
    void *smbios_table;
    size_t smbios_table_size;
};

static void *efi_search_guid(EFI_SYSTEM_TABLE *tab, EFI_GUID *guid)
{
	uint64_t i;
	void *pVendortable = NULL;

	if (tab != NULL) {
		for (i = 0; i < tab->NumberOfTableEntries; i++) {
			EFI_CONFIGURATION_TABLE *conf_tab = &tab->ConfigurationTable[i];

			if (uuid_is_equal((uint8_t *)&conf_tab->VendorGuid, (uint8_t *)guid)) {
				pVendortable = hpa2hva((uint64_t)conf_tab->VendorTable);
				break;
			}
		}
	}

	return pVendortable;
}

static inline void get_smbios3_info(struct smbios3_entry_point *eps3, struct smbios_info *si)
{
    si->smbios_eps_size = eps3->length;
    memcpy_s(&si->smbios_eps, si->smbios_eps_size, eps3, si->smbios_eps_size);
    si->smbios_table = hpa2hva(eps3->st_addr);
    si->smbios_table_size = eps3->max_st_size;
}

static inline void get_smbios2_info(struct smbios2_entry_point *eps2, struct smbios_info *si)
{
    si->smbios_eps_size = eps2->length;
    memcpy_s(&si->smbios_eps, si->smbios_eps_size, eps2, si->smbios_eps_size);
    si->smbios_table = hpa2hva(eps2->st_addr);
    si->smbios_table_size = eps2->st_length;
}

static void generate_checksum(uint8_t *byte_start, int nbytes, uint8_t *checksum_pos)
{
    *checksum_pos = 0;
    *checksum_pos = -calculate_sum8(byte_start, nbytes);
}

static int efi_search_smbios_eps(EFI_SYSTEM_TABLE *efi_system_table, struct smbios_info *si)
{
    void *p = NULL;
    EFI_GUID smbios3_guid = SMBIOS3_TABLE_GUID;
    EFI_GUID smbios2_guid = SMBIOS2_TABLE_GUID;

    /* If both are present, SMBIOS3 takes precedence over SMBIOS */
    stac();
    p = efi_search_guid(efi_system_table, &smbios3_guid);
    if (p != NULL) {
        get_smbios3_info((struct smbios3_entry_point *)p, si);
    } else {
        p = efi_search_guid(efi_system_table, &smbios2_guid);
        if (p != NULL) {
            get_smbios2_info((struct smbios2_entry_point *)p, si);
        }
    }
    clac();

    return (p != NULL);
}

static int copy_smbios_to_guest(struct acrn_vm *vm, struct smbios_info *si)
{
    int ret = 0;
    uint64_t gpa;

    gpa = VIRT_SMBIOS_TABLE_ADDR;
    ret = copy_to_gpa(vm, si->smbios_table, gpa, si->smbios_table_size);
    if (ret == 0) {
        if (strncmp("_SM_", si->smbios_eps.eps2.anchor, 4) == 0) {
            struct smbios2_entry_point *eps2 = &si->smbios_eps.eps2;
            eps2->st_addr = (uint32_t)gpa;
            /* If we wrote generate_checksum(eps->int_anchor, ...), the code scanning tool will
             * emit warnings about array bound overflow. So use offsetof instead.
             */
            generate_checksum((uint8_t *)eps2 + offsetof(struct smbios2_entry_point, int_anchor),
                0xf, &eps2->int_checksum);
            generate_checksum((uint8_t *)eps2, eps2->length, &eps2->checksum);
        } else if (strncmp("_SM3_", si->smbios_eps.eps3.anchor, 5) == 0) {
            struct smbios3_entry_point *eps3 = &si->smbios_eps.eps3;
            eps3->st_addr = (uint32_t)gpa;
            generate_checksum((uint8_t *)eps3, eps3->length, &eps3->checksum);
        }

        gpa = VIRT_SMBIOS_EPS_ADDR;
        ret = copy_to_gpa(vm, &si->smbios_eps, gpa, si->smbios_eps_size);
    }

    return ret;
}

static int is_smbios3_present(uint8_t *p)
{
    return (strncmp((const char *)p, "_SM3_", 5) == 0 &&
        (calculate_sum8(p, ((struct smbios3_entry_point *)p)->length)) == 0);
}

static int is_smbios2_present(uint8_t *p)
{
    return (strncmp((const char *)p, "_SM_", 4) == 0 &&
        (calculate_sum8(p, ((struct smbios2_entry_point *)p)->length)) == 0);
}

static int scan_smbios_eps(struct smbios_info *si)
{
    uint8_t *start = (uint8_t *)hpa2hva(SMBIOS_EPS_SEARCH_START);
    uint8_t *end = (uint8_t *)hpa2hva(SMBIOS_EPS_SEARCH_END);
    uint8_t *p;

    /* per SMBIOS spec 3.2.0, 32-bit (SMBIOS) and 64-bit (SMBIOS3) EPS can be found by searching
     * for the anchor string on paragraph (16-byte) boundaries within the physical address
     * 0xf0000-0xfffff.
     */
    stac();
    for (p = start; p < end; p += 16) {
        if (is_smbios3_present(p)) {
            get_smbios3_info((struct smbios3_entry_point *)p, si);
            break;
        } else if (is_smbios2_present(p)) {
            get_smbios2_info((struct smbios2_entry_point *)p, si);
            break;
        }
    }
    clac();

    return (p < end);
}

static int probe_smbios_table(struct acrn_boot_info *abi, struct smbios_info *si)
{
    int found = 0;

    if (boot_from_uefi(abi)) {
        /* Get from EFI system table */
        uint64_t efi_system_tab_paddr = ((uint64_t)abi->uefi_info.systab_hi << 32) | ((uint64_t)abi->uefi_info.systab);
        EFI_SYSTEM_TABLE *efi_system_tab = (EFI_SYSTEM_TABLE *)hpa2hva(efi_system_tab_paddr);
        found = efi_search_smbios_eps(efi_system_tab, si);
    } else {
        /* Search eps in 0xf0000-0xfffff */
        found = scan_smbios_eps(si);
    }
    /* Note: Multiboot2 spec specifies an SMBIOS tag where the bootloader can pass an "SMBIOS table" to OS.
     * As of today GRUB does not support this feature, and if they were to support it, they will do the same
     * thing we did here, i.e.: either reading from EFI system table or scan 0xf0000~0xfffff. So we will skip
     * trying to get SMBIOS table from Multiboot2 tag.
     */

#ifdef VM0_TPM_EVENTLOG_BASE_ADDR
    if (found && (si->smbios_table_size > (VM0_TPM_EVENTLOG_BASE_ADDR - VIRT_SMBIOS_TABLE_ADDR))) {
        /* Unlikely but we check this anyway */
        pr_err("Error: SMBIOS table too large. Stop copying SMBIOS info to guest.");
        found = 0;
    }
#endif

    return found;
}

void passthrough_smbios(struct acrn_vm *vm, struct acrn_boot_info *abi)
{
    struct smbios_info si;
	struct acrn_vm_config *vm_config = get_vm_config(vm->vm_id);

    if (is_prelaunched_vm(vm) && ((vm_config->guest_flags & GUEST_FLAG_SECURITY_VM) != 0)) {
        memset(&si, 0, sizeof(struct smbios_info));

        if (probe_smbios_table(abi, &si)) {
            if (copy_smbios_to_guest(vm, &si)) {
                pr_err("Failed to copy SMBIOS info to vm%d", vm->vm_id);
            }
        }
    }
}
