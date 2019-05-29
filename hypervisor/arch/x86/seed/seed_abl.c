/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <cpu.h>
#include <pgtable.h>
#include <rtl.h>
#include <seed.h>

#define ABL_SEED_LEN 32U
struct abl_seed_info {
	uint8_t svn;
	uint8_t reserved[3];
	uint8_t seed[ABL_SEED_LEN];
};

#define ABL_SEED_LIST_MAX 4U
struct abl_svn_seed {
	uint32_t size_of_this_struct;
	uint32_t version;
	uint32_t num_seeds;
	struct abl_seed_info seed_list[ABL_SEED_LIST_MAX];
};

/*
 * parse_seed_abl
 *
 * description:
 *    This function parse seed_list which provided by ABL.
 *
 * input:
 *    cmdline       pointer to cmdline string
 *
 * output:
 *    phy_seed      pointer to physical seed structure
 *
 * return value:
 *    true if parse successfully, otherwise false.
 */
bool parse_seed_abl(uint64_t addr, struct physical_seed *phy_seed)
{
	uint32_t i;
	uint32_t legacy_seed_index = 0U;
	struct seed_info *seed_list;
	struct abl_svn_seed *abl_seed = (struct abl_svn_seed *)hpa2hva(addr);
	bool status = false;

	if ((phy_seed != NULL) && (abl_seed != NULL) &&
	    (abl_seed->num_seeds >= 2U) && (abl_seed->num_seeds <= ABL_SEED_LIST_MAX)) {

		seed_list = phy_seed->seed_list;
		/*
		 * The seed_list from ABL contains several seeds which based on SVN
		 * and one legacy seed which is not based on SVN. The legacy seed's
		 * svn value is minimum in the seed list. And CSE ensures at least two
		 * seeds will be generated which will contain the legacy seed.
		 * Here find the legacy seed index first.
		 */
		for (i = 1U; i < abl_seed->num_seeds; i++) {
			if (abl_seed->seed_list[i].svn < abl_seed->seed_list[legacy_seed_index].svn) {
				legacy_seed_index = i;
			}
		}

		/*
		 * Copy out abl_seed for trusty and clear the original seed in memory.
		 * The SOS requires the legacy seed to derive RPMB key. So skip the
		 * legacy seed when clear original seed.
		 */
		(void)memset((void *)&phy_seed->seed_list[0U], 0U, sizeof(phy_seed->seed_list));
		for (i = 0U; i < abl_seed->num_seeds; i++) {
			seed_list[i].cse_svn = abl_seed->seed_list[i].svn;
			(void)memcpy_s((void *)&seed_list[i].seed[0U], sizeof(seed_list[i].seed),
					(void *)&abl_seed->seed_list[i].seed[0U], sizeof(abl_seed->seed_list[i].seed));

			if (i == legacy_seed_index) {
				continue;
			}

			(void)memset((void *)&abl_seed->seed_list[i].seed[0U], 0U,
							sizeof(abl_seed->seed_list[i].seed));
		}

		phy_seed->num_seeds = abl_seed->num_seeds;
		status = true;
	}

	return status;
}
