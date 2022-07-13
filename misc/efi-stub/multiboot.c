/*
 * Copyright (c) 2021 - 2022, Intel Corporation.
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
#include "multiboot.h"

/**
 * @brief Search the first len bytes in buffer for multiboot1 header.
 *
 * @param[in] buffer Buffer to be searched
 * @param[in] len    Search length
 *
 * @return A pointer to the multiboot1 header if found. NULL otherwise.
 */
const struct multiboot_header *find_mb1header(const UINT8 *buffer, uint64_t len)
{
	const struct multiboot_header *header;

	for (header = (struct multiboot_header *)buffer;
		((char *)header <= (char *)buffer + len - 12);
		header = (struct multiboot_header *)((char *)header + MULTIBOOT_HEADER_ALIGN))
	{
		if (header->mh_magic == MULTIBOOT_HEADER_MAGIC &&
			!(header->mh_magic + header->mh_flags + header->mh_checksum))
			return header;
	}

	return NULL;
}

/**
 * @brief Search the first len bytes in buffer for multiboot2 header.
 *
 * @param[in] buffer Buffer to be searched
 * @param[in] len    Search length
 *
 * @return A pointer to the multiboot2 header if found. NULL otherwise.
 */
const struct multiboot2_header *find_mb2header(const UINT8 *buffer, uint64_t len)
{
	const struct multiboot2_header *header;

	for (header = (const struct multiboot2_header *)buffer;
		((char *)header <= (char *)buffer + len - 12);
		header = (struct multiboot2_header *)((uint64_t)header + MULTIBOOT2_HEADER_ALIGN / 4))
	{
		if (header->magic == MULTIBOOT2_HEADER_MAGIC &&
			!(header->magic + header->architecture + header->header_length + header->checksum) &&
			header->architecture == MULTIBOOT2_ARCHITECTURE_I386)
			return header;
	}

	return NULL;
}

/**
 * @brief Parse the multiboot2 header and return a list of pointers to the header tags.
 *
 * @param[in]  header     Multiboot2 header to be parsed.
 * @param[out] hv_tags    An hv_mb2header_tag_list struct that contains pointers to all possible
 *                        tags in a multiboot2 header. If a field in this struct is not NULL, it
 *                        means the tag was found in the given header. NULL otherwise.
 *
 * @return 0 on success. -1 on error.
 */
int parse_mb2header(const struct multiboot2_header *header, struct hv_mb2header_tag_list *hv_tags)
{
	struct multiboot2_header_tag *tag;

	memset(hv_tags, 0, sizeof(struct hv_mb2header_tag_list));

	for (tag = (struct multiboot2_header_tag *)(header + 1);
		tag->type != MULTIBOOT2_TAG_TYPE_END;
		tag = (struct multiboot2_header_tag *)((uint32_t *)tag + ALIGN_UP(tag->size, MULTIBOOT2_TAG_ALIGN) / 4))
	{
		switch (tag->type) {
			case MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST:
				/* Ignored. Currently we didn't support all categories of requested information,
				 * only the part that ACRN requests. So we don't parse the requests here. */
				break;

			case MULTIBOOT2_HEADER_TAG_ADDRESS:
				hv_tags->addr = (struct multiboot2_header_tag_address *)tag;
				break;

			case MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS:
				hv_tags->entry = (struct multiboot2_header_tag_entry_address *)tag;
				break;

			case MULTIBOOT2_HEADER_TAG_RELOCATABLE:
				hv_tags->reloc = (struct multiboot2_header_tag_relocatable *)tag;
				break;

			default:
				Print(L"Unsupported multiboot2 tag type: %d\n", tag->type);
				return -1;
		}
	}

	if (hv_tags->addr && !hv_tags->entry)
		return -1;

	return 0;
}
