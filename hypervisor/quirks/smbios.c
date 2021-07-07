/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <types.h>
#include <util.h>
#include <asm/guest/vm.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/guest/ept.h>
#include <acrn_common.h>
#include <debug/logmsg.h>

#define SMBIOS_TABLE_GUID {0xeb9d2d31, 0x2d88, 0x11d3, {0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}}
#define SMBIOS3_TABLE_GUID {0xf2fd1544, 0x9794, 0x4a2c, {0x99,0x2e,0xe5,0xbb,0xcf,0x20,0xe3,0x94}}

struct smbios_entry_point {
    char anchor[4];			/* "_SM_" */
	uint8_t checksum;
	uint8_t length;			/* length of this entry point structure, currently 1Fh */
	uint8_t major_ver;		/* version major */
	uint8_t minor_ver;		/* version minor */
	uint16_t max_struct_size;	/* size of the largest SMBIOS structure */
	uint8_t ep_rev;			/* entry point structure revision */
	uint8_t formatted[5];
	char int_anchor[5];		/* "_DMI_" */
	uint8_t int_checksum;	/* intermediate checksum */
	uint16_t st_length;		/* total length of SMBIOS structure table */
	uint32_t st_addr;		/* structure table address */
	uint16_t nstructs;		/* number of SMBIOS structures */
	uint8_t bcd_revision;	/* BCD revision */
} __attribute__((packed));

struct smbios3_entry_point {
    char anchor[5];			/* "_SM3_" */
	uint8_t checksum;
	uint8_t length;			/* length of this entry point structure, currently 18h */
	uint8_t major_ver;
	uint8_t minor_ver;
	uint8_t docrev;
	uint8_t ep_rev;
	uint8_t reserved;
	uint32_t max_st_size;	/* max structure table size. The actual size is guaranteed to be <= this value. */
	uint64_t st_addr;		/* structure table address */
} __attribute__((packed));

typedef struct {
    uint32_t  Data1;
    uint16_t  Data2;
    uint16_t  Data3;
    uint8_t   Data4[8];
} EFI_GUID;

typedef struct _EFI_CONFIGURATION_TABLE {
    EFI_GUID VendorGuid;
    void *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct _EFI_TABLE_HEADER {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    uint16_t *FirmwareVendor;
    uint32_t FirmwareRevision;
    void *ConsoleInHandle;
    void *ConIn;
    void *ConsoleOutHandle;
    void *ConOut;
    void *StandardErrorHandle;
    void *StdErr;
    void *RuntimeServices;
    void *BootServices;
    uint64_t NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;

union {
    struct smbios_entry_point eps;
    struct smbios3_entry_point eps3;
} smbios_eps;
size_t smbios_eps_size;
void *smbios_table;
size_t smbios_table_size;

static int memcmp(uint8_t *s1, uint8_t *s2, size_t len)
{
    while (len-- > 0) {
        if (*s1++ != *s2++) {
            return s1[-1] < s2[-1] ? -1 : 1;
        }
    }
    return 0;
}

static void *detect_smbios_eps(EFI_SYSTEM_TABLE *efi_system_tab, EFI_GUID *target_guid)
{
    uint64_t i;
    void *smbios_eps = NULL;

    /* search EFI system table -> configuration table */
    for (i = 0; i < efi_system_tab->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *conf_tab = &efi_system_tab->ConfigurationTable[i];
        EFI_GUID guid = conf_tab->VendorGuid;

        if (!memcmp((uint8_t *)&guid, (uint8_t *)target_guid, sizeof(EFI_GUID))) {
            smbios_eps = hpa2hva((uint64_t)conf_tab->VendorTable);
            break;
        }
    }

    /* We don't search for legacy 0xf0000~0xfffff region because UEFI environment is assumed. */
    return smbios_eps;
}

static void smbios_table_probe(struct acrn_boot_info *abi)
{
    void *p;
    EFI_GUID smbios3_guid = SMBIOS3_TABLE_GUID;
    EFI_GUID smbios_guid = SMBIOS_TABLE_GUID;
    uint64_t efi_system_tab_paddr = (uint64_t)abi->efi_info.system_table;
    EFI_SYSTEM_TABLE *efi_system_tab = (EFI_SYSTEM_TABLE *)hpa2hva(efi_system_tab_paddr);

    stac();
    /* If both are present, SMBIOS3 takes precedence over SMBIOS */
    p = detect_smbios_eps(efi_system_tab, &smbios3_guid);
    if (p) {
        struct smbios3_entry_point *eps = (struct smbios3_entry_point *)p;
        smbios_eps_size = eps->length;
        memcpy_s(&smbios_eps, smbios_eps_size, eps, smbios_eps_size);
        smbios_table = hpa2hva(eps->st_addr);
        smbios_table_size = eps->max_st_size;
    } else {
        p = detect_smbios_eps(efi_system_tab, &smbios_guid);
        if (p) {
            struct smbios_entry_point *eps = (struct smbios_entry_point *)p;
            smbios_eps_size = eps->length;
            memcpy_s(&smbios_eps, smbios_eps_size, eps, smbios_eps_size);
            smbios_table = hpa2hva(eps->st_addr);
            smbios_table_size = eps->st_length;
        }
    }
    clac();
}

/* Recalculate checksum of n bytes starting from byte_start, and write checksum to checksum_pos */
static void recalc_checksum(uint8_t *byte_start, uint8_t nbytes, uint8_t *checksum_pos)
{
    int i;
    uint8_t sum = 0;
    for (i = 0; i < nbytes; i++) {
        if (byte_start + i != checksum_pos) {
            sum += byte_start[i];
        }
    }
    *checksum_pos = -sum;
}

static int copy_smbios_to_guest(struct acrn_vm *vm)
{
    uint64_t gpa;
    int ret = -1;

    if (smbios_eps_size) {
        /* SMBIOS (32-bit entry point) requires the table to be below 4GB, whereas the
         * SMBIOS3 (64-bit entry point) requires the table to be in anywhere in 64-bit
         * memory address. So we put it below 4GB.
         * Note that find_space_from_ve820 does not mark space as reserved as it gives out
         * address ranges, so we need to check with other usages of this API (basically
         * kernel and module loading).
         */
        uint64_t klend = (uint64_t)vm->sw.kernel_info.kernel_load_addr + vm->sw.kernel_info.kernel_size + MEM_2M;
        uint64_t mlend = max((uint64_t)vm->sw.ramdisk_info.load_addr + vm->sw.ramdisk_info.size, MEM_1M);
        uint64_t high = max(klend, mlend);
        gpa = high + 0x1000;
        if (gpa != INVALID_GPA) {
            ret = copy_to_gpa(vm, smbios_table, gpa, smbios_table_size);
            if (ret == 0) {
                if (smbios_eps.eps.anchor[3] == '_') {
                    /* SMBIOS (_SM_) */
                    struct smbios_entry_point *eps = &smbios_eps.eps;
                    eps->st_addr = (uint32_t)gpa;
                    /* the intermediate checksum covers the structure table field */
                    recalc_checksum((uint8_t *)eps->int_anchor, 0xf, &eps->int_checksum);
                } else {
                    /* SMBIOS3 (_SM3_) */
                    struct smbios3_entry_point *eps3 = &smbios_eps.eps3;
                    eps3->st_addr = (uint32_t)gpa;
                    recalc_checksum((uint8_t *)eps3, eps3->length, &eps3->checksum);
                }

                /* The SMBIOS entry point structure should reside in between 0xf0000~0xfffff */
                /* The rsdp starts from 0xf2400 so 0xf1000 should be OK. This structure is at most 31 bytes. */
                gpa = 0xf1000UL;
                ret = copy_to_gpa(vm, &smbios_eps, gpa, smbios_eps_size);
            }
        }
    }

    return ret;
}

void try_smbios_passthrough(struct acrn_vm *vm, struct acrn_boot_info *abi)
{
    /* TODO: Add config and guest flag to disable it by default */
    if (is_prelaunched_vm(vm)) {
        smbios_table_probe(abi);
        (void)copy_smbios_to_guest(vm);
    }
}
