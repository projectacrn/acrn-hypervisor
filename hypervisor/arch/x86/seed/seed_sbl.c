/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <seed.h>
#include "seed_sbl.h"

#define SEED_ENTRY_TYPE_SVNSEED         0x1U
/* #define SEED_ENTRY_TYPE_RPMBSEED        0x2U */

/* #define SEED_ENTRY_USAGE_USEED          0x1U */
#define SEED_ENTRY_USAGE_DSEED          0x2U

struct seed_list_hob {
	uint8_t revision;
	uint8_t reserved0[3];
	uint32_t buffer_size;
	uint8_t total_seed_count;
	uint8_t reserved1[3];
};

struct seed_entry {
	/* SVN based seed or RPMB seed or attestation key_box */
	uint8_t type;
	/* For SVN seed: useed or dseed
	 * For RPMB seed: serial number based or not
	 */
	uint8_t usage;
	/* index for the same type and usage seed */
	uint8_t index;
	uint8_t reserved;
	/* reserved for future use */
	uint16_t flags;
	/* Total size of this seed entry */
	uint16_t seed_entry_size;
	/* SVN seed: struct seed_info
	 * RPMB seed: uint8_t rpmb_key[key_len]
	 */
	uint8_t seed[0];
};

/*
 * parse_seed_sbl
 *
 * description:
 *    This function parse seed_list which provided by SBL
 *
 * input:
 *    cmdline       pointer to cmdline string
 *
 * return value:
 *    true if parse successfully, otherwise false.
 */
bool parse_seed_sbl(uint64_t addr, struct physical_seed *phy_seed)
{
	uint8_t i;
	uint8_t dseed_index = 0U;
	struct image_boot_params *boot_params;
	struct seed_list_hob *seed_hob = NULL;
	struct seed_entry *entry;
	struct seed_info *seed_list;
	bool status = false;

	stac();

	boot_params = (struct image_boot_params *)hpa2hva(addr);

	if ((boot_params != NULL) || (phy_seed != NULL)) {
		seed_hob = (struct seed_list_hob *)hpa2hva(boot_params->p_seed_list);
	}

	if (seed_hob != NULL) {
		status = true;

		seed_list = phy_seed->seed_list;

		entry = (struct seed_entry *)((uint8_t *)seed_hob + sizeof(struct seed_list_hob));

		for (i = 0U; i < seed_hob->total_seed_count; i++) {
			if (entry != NULL) {
				/* retrieve dseed */
				if ((SEED_ENTRY_TYPE_SVNSEED == entry->type) &&
				    (SEED_ENTRY_USAGE_DSEED == entry->usage)) {

					/* The seed_entry with same type/usage are always
					 * arranged by index in order of 0~3.
					 */
					if ((entry->index != dseed_index) ||
					    (entry->index >= BOOTLOADER_SEED_MAX_ENTRIES)) {
						pr_warn("%s: Invalid seed index.", __func__);
						status = false;
						break;
					}

					(void)memcpy_s((void *)&seed_list[dseed_index], sizeof(struct seed_info),
							(void *)&entry->seed[0U], sizeof(struct seed_info));
					dseed_index++;

					/* erase original seed in seed entry */
					(void)memset((void *)&entry->seed[0U], 0U, sizeof(struct seed_info));
				}
			}

			entry = (struct seed_entry *)((uint8_t *)entry + entry->seed_entry_size);
		}

		if (status) {
			phy_seed->num_seeds = dseed_index;
		}
	}

	clac();

	return status;
}
