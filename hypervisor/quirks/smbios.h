/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QUIRKS_SMBIOS_H
#define QUIRKS_SMBIOS_H

#include <asm/guest/vm.h>
#include <boot.h>

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

struct smbios2_entry_point {
    char anchor[4];			/* "_SM_" */
	uint8_t checksum;		/* covers the entire struct */
	uint8_t length;			/* length of this entry point structure, currently 1Fh */
	uint8_t major_ver;		/* version major */
	uint8_t minor_ver;		/* version minor */
	uint16_t max_struct_size;	/* size of the largest SMBIOS structure */
	uint8_t ep_rev;			/* entry point structure revision */
	uint8_t formatted[5];
	char int_anchor[5];		/* "_DMI_" */
	uint8_t int_checksum;	/* intermediate checksum, covers from start of int_anchor to bcd_revision */
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

#define SMBIOS2_TABLE_GUID {0xeb9d2d31, 0x2d88, 0x11d3, {0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}}
#define SMBIOS3_TABLE_GUID {0xf2fd1544, 0x9794, 0x4a2c, {0x99,0x2e,0xe5,0xbb,0xcf,0x20,0xe3,0x94}}

#endif
