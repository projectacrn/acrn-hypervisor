/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <hob_parse.h>

void parse_seed_list(struct seed_list_hob *seed_hob)
{
	uint8_t i;
	uint8_t dseed_index = 0U;
	struct seed_entry *entry;
	struct seed_info dseed_list[BOOTLOADER_SEED_MAX_ENTRIES];

	if (!seed_hob) {
		pr_warn("Invalid seed_list hob pointer. Use fake seed!");
		goto fail;
	}

	if (seed_hob->total_seed_count == 0U) {
		pr_warn("Total seed count is 0. Use fake seed!");
		goto fail;
	}

	entry = (struct seed_entry *)((uint8_t *)seed_hob +
					sizeof(struct seed_list_hob));

	for (i = 0U; i < seed_hob->total_seed_count; i++) {
		/* retrieve dseed */
		if ((SEED_ENTRY_TYPE_SVNSEED == entry->type) &&
			(SEED_ENTRY_USAGE_DSEED == entry->usage)) {

			/* The seed_entry with same type/usage are always
			 * arranged by index in order of 0~3.
			 */
			if (entry->index != dseed_index) {
				pr_warn("Index mismatch. Use fake seed!");
				goto fail;
			}

			if (entry->index >= BOOTLOADER_SEED_MAX_ENTRIES) {
				pr_warn("Index exceed max number!");
				goto fail;
			}

			(void)memcpy_s(&dseed_list[dseed_index],
					sizeof(struct seed_info),
					entry->seed,
					sizeof(struct seed_info));
			dseed_index++;

			/* erase original seed in seed entry */
			(void)memset(entry->seed, 0U, sizeof(struct seed_info));
		}

		entry = (struct seed_entry *)((uint8_t *)entry +
						entry->seed_entry_size);
	}

	trusty_set_dseed(dseed_list, dseed_index);
	(void)memset(dseed_list, 0U, sizeof(dseed_list));
	return;

fail:
	trusty_set_dseed(NULL, 0U);
	(void)memset(dseed_list, 0U, sizeof(dseed_list));
}
