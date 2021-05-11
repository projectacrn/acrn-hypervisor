/*
 * Copyright (c) 2017 - 2021, Intel Corporation. All rights reserved.
 * SPDX-License-Identifier: BSD-2-Clause-Patent
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
 */


#include <efi.h>
#include <efilib.h>
#include "boot.h"
#include "stdlib.h"
#include "container.h"

EFI_STATUS parse_container_image(EFI_LOADED_IMAGE *info)
{
	EFI_STATUS err = EFI_SUCCESS;
	char *section  = NULL;
	UINTN sec_addr = 0u;
	UINTN sec_size = 0u;

	int i;
	int j;
	CONTAINER_HDR   *hdr  = NULL;
	COMPONENT_ENTRY *comp = NULL;

	section = ".os";
	err = get_pe_section(info->ImageBase, section, strlen(section), &sec_addr, &sec_size);
	if (EFI_ERROR(err)) {
		Print(L"Unable to locate section of ACRNHV Container %r ", err);
		goto exit;
	}

	hdr = (CONTAINER_HDR*)(info->ImageBase + sec_addr);

	Print(L"\n");
	Print(L"container   : name = %c%c%c%c, ", ((char*)&hdr->Signature)[0], ((char*)&hdr->Signature)[1],
							((char*)&hdr->Signature)[2], ((char*)&hdr->Signature)[3]);
	Print(L"offset = %08xh, size = %08lu\n", sec_addr, sec_size);

	comp = (COMPONENT_ENTRY *)(hdr + 1);
	for (i = 0; i < hdr->Count; i++) {
		const UINTN offset = hdr->DataOffset + comp->Offset;
		LOADER_COMPRESSED_HEADER *lzh = (LOADER_COMPRESSED_HEADER *)((UINT8 *)(hdr) + offset);

		Print(L"component[%d]: name = %c%c%c%c, offset = %08xh, size = %08lu", i,
									((char*)&comp->Name)[0], ((char*)&comp->Name)[1],
									((char*)&comp->Name)[2], ((char*)&comp->Name)[3],
									offset, lzh->Size);
		if ((i % 2) == 0) {
			if (i < hdr->Count - 1) {
				Print(L", tag = ");
				for (j = 0; j < lzh->Size; j++) {
					Print(L"%c", lzh->Data[j]);
				}
			}
		} else {
			Print(L"\n");
		}

		comp = (COMPONENT_ENTRY *)((UINT8 *)(comp + 1) + comp->HashSize);
	}

exit:
	return err;
}
