/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <boot.h>
#include <pgtable.h>
#include <util.h>
#include <logmsg.h>

/**
 * @pre mbi != NULL && mb2_tag_mmap != NULL
 */
static void mb2_mmap_to_mbi(struct acrn_multiboot_info *mbi, struct multiboot2_tag_mmap *mb2_tag_mmap)
{
	uint32_t i;

	/* multiboot2 mmap tag header occupied 16 bytes */
	mbi->mi_mmap_entries = (mb2_tag_mmap->size - 16U) / sizeof(struct multiboot2_mmap_entry);
	if (mbi->mi_mmap_entries > E820_MAX_ENTRIES) {
		pr_err("Too many E820 entries %d\n", mbi->mi_mmap_entries);
		mbi->mi_mmap_entries = E820_MAX_ENTRIES;
	}
	for (i = 0U; i < mbi->mi_mmap_entries; i++) {
		mbi->mi_mmap_entry[i].baseaddr = mb2_tag_mmap->entries[i].addr;
		mbi->mi_mmap_entry[i].length = mb2_tag_mmap->entries[i].len;
		mbi->mi_mmap_entry[i].type = mb2_tag_mmap->entries[i].type;
	}
	mbi->mi_flags |= MULTIBOOT_INFO_HAS_MMAP;
}

/**
 * @pre mbi != NULL && mb2_info != NULL
 */
int32_t multiboot2_to_acrn_mbi(struct acrn_multiboot_info *mbi, void *mb2_info)
{
	int32_t ret = 0;
	struct multiboot2_tag *mb2_tag, *mb2_tag_end;
	uint32_t mb2_info_size = *(uint32_t *)mb2_info;

	/* The start part of multiboot2 info: total mbi size (4 bytes), reserved (4 bytes) */
	mb2_tag = (struct multiboot2_tag *)((uint8_t *)mb2_info + 8U);
	mb2_tag_end = (struct multiboot2_tag *)((uint8_t *)mb2_info + mb2_info_size);

	while ((mb2_tag->type != MULTIBOOT2_TAG_TYPE_END) && (mb2_tag < mb2_tag_end)) {
		if (mb2_tag->size == 0U) {
			pr_err("the multiboot2 tag size should not be 0!");
			ret = -EINVAL;
			break;
		}

		switch (mb2_tag->type) {
		case MULTIBOOT2_TAG_TYPE_MMAP:
			mb2_mmap_to_mbi(mbi, (struct multiboot2_tag_mmap *)mb2_tag);
			break;
		default:
			if (mb2_tag->type <= MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR) {
				pr_warn("unhandled multiboot2 tag type: %d", mb2_tag->type);
			} else {
				pr_err("unknown multiboot2 tag type: %d", mb2_tag->type);
				ret = -EINVAL;
			}
			break;
		}
		if (ret != 0) {
			pr_err("multiboot2 info format error!");
			break;
		}
		/*
		 * tag->size is not including padding whearas each tag
		 * start at 8-bytes aligned address.
		 */
		mb2_tag = (struct multiboot2_tag *)((uint8_t *)mb2_tag
				+ ((mb2_tag->size + (MULTIBOOT2_INFO_ALIGN - 1U)) & ~(MULTIBOOT2_INFO_ALIGN - 1U)));
	}
	return ret;
}
