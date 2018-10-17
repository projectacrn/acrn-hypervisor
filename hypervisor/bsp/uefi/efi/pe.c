/*
 * Copyright (c) 2011, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file contains some wrappers around the gnu-efi functions. As
 * we're not going through uefi_call_wrapper() directly, this allows
 * us to get some type-safety for function call arguments and for the
 * compiler to check that the number of function call arguments is
 * correct.
 *
 * It's also a good place to document the EFI interface.
 */

#include <efi.h>
#include <efilib.h>
#include "stdlib.h"

#define	DOS_FILE_MAGIC_NUMBER	0x5A4D  /* "MZ" */
struct DosFileHeader {
	uint16_t  mMagic;
        uint16_t  LastSize;
        uint16_t  nBlocks;
        uint16_t  nReloc;
        uint16_t  HdrSize;
        uint16_t  MinAlloc;
        uint16_t  MaxAlloc;
        uint16_t  ss;
        uint16_t  sp;
        uint16_t  Checksum;
        uint16_t  ip;
        uint16_t  cs;
        uint16_t  RelocPos;
        uint16_t  nOverlay;
        uint16_t  reserved[4];
        uint16_t  OEMId;
        uint16_t  OEMInfo;
        uint16_t  reserved2[10];
        uint32_t  ExeHeader;
} __attribute__((packed));

#define IMAGE_FILE_MACHINE_I386	0x14c
#define IMAGE_FILE_MACHINE_AMD64    0x8664
#define PE_FILE_MAGIC_NUMBER	0x00004550 	/* "PE\0\0" */
struct PeHeader {
	uint32_t mMagic; 
	uint16_t mMachine;
	uint16_t mNumberOfSections;
	uint32_t mTimeDateStamp;
	uint32_t mPointerToSymbolTable;
	uint32_t mNumberOfSymbols;
	uint16_t mSizeOfOptionalHeader;
	uint16_t mCharacteristics;
} __attribute__((packed));

struct OptionHeader {
	uint16_t Format;
	uint8_t  MajorLinkVer;
	uint8_t  MinorLinkVer;
	uint32_t  CodeSize;
	uint32_t  InitializedDataSize;
	uint32_t  UninitializedDataSize;
	uint32_t  EntryPoint;
	uint32_t  BaseOfCode;
	uint32_t  BaseOfDate;
} __attribute__((packed));


struct PeSectionHeader {
	char  mName[8];
	uint32_t mVirtualSize;
	uint32_t mVirtualAddress;
	uint32_t mSizeOfRawData;
	uint32_t mPointerToRawData;
	uint32_t mPointerToRealocations;
	uint32_t mPointerToLinenumbers;
	uint16_t mNumberOfRealocations;
	uint16_t mNumberOfLinenumbers;
	uint32_t mCharacteristics;
} __attribute__((packed));


EFI_STATUS get_pe_section(CHAR8 *base, char *section, UINTN *vaddr, UINTN *size)
{
        struct PeSectionHeader *ph;
        struct DosFileHeader *dh;
        struct PeHeader *pe;
        UINTN i;
        UINTN offset;

        dh = (struct DosFileHeader *)base;

        if (dh->mMagic != DOS_FILE_MAGIC_NUMBER)
                return EFI_LOAD_ERROR;

        pe = (struct PeHeader *)&base[dh->ExeHeader];
		if (pe->mMagic != PE_FILE_MAGIC_NUMBER)
                return EFI_LOAD_ERROR;

        if ((pe->mMachine != IMAGE_FILE_MACHINE_AMD64)
           && (pe->mMachine != IMAGE_FILE_MACHINE_I386))
                return EFI_LOAD_ERROR;

        offset = dh->ExeHeader + sizeof(*pe) + pe->mSizeOfOptionalHeader;

        for (i = 0; i < pe->mNumberOfSections; i++) {
                ph = (struct PeSectionHeader *)&base[offset];
                if (CompareMem(ph->mName, section, strlen(section)) == 0) {
			*vaddr = (UINTN)ph->mVirtualAddress;
                         *size = (UINTN)ph->mVirtualSize;
			break;
		}

                offset += sizeof(*ph);
        }

        return EFI_SUCCESS;
}


EFI_IMAGE_ENTRY_POINT get_pe_entry(CHAR8 *base)
{
	struct DosFileHeader* dh;
	struct PeHeader* pe;
	struct OptionHeader* oh;
	UINTN offset;

	dh = (struct DosFileHeader *)base;

	if (dh->mMagic != DOS_FILE_MAGIC_NUMBER)
		return NULL;

	pe = (struct PeHeader *)&base[dh->ExeHeader];
	if (pe->mMagic != PE_FILE_MAGIC_NUMBER)
		return NULL;

	if ((pe->mMachine != IMAGE_FILE_MACHINE_AMD64)
		&& (pe->mMachine != IMAGE_FILE_MACHINE_I386))
		return NULL;

    offset = dh->ExeHeader + sizeof(*pe);
	oh = (struct OptionHeader*)&base[offset];

	return (EFI_IMAGE_ENTRY_POINT)((UINT64)base + oh->EntryPoint);
}
