/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SEED_SBL_H_
#define SEED_SBL_H_

struct image_boot_params {
	uint32_t size_of_this_struct;
	uint32_t version;
	uint64_t p_seed_list;
	uint64_t p_platform_info;
	uint64_t reserved;
};

bool parse_seed_sbl(uint64_t addr, struct physical_seed *phy_seed);

#endif /* SEED_SBL_H_ */
