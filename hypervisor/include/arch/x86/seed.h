/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SEED_H_
#define SEED_H_

#define BOOTLOADER_SEED_MAX_ENTRIES     10U
#define BUP_MKHI_BOOTLOADER_SEED_LEN    64U

/* Structure of seed info */
struct seed_info {
	uint8_t cse_svn;
	uint8_t bios_svn;
	uint8_t padding[2];
	uint8_t seed[BUP_MKHI_BOOTLOADER_SEED_LEN];
};

/* Structure of physical seed */
struct physical_seed {
	struct seed_info seed_list[BOOTLOADER_SEED_MAX_ENTRIES];
	uint32_t num_seeds;
	uint32_t pad;
};

void init_seed(void);

void append_seed_arg(char *cmd_dst, bool vm_is_sos);

bool derive_virtual_seed(struct seed_info *seed_list, uint32_t *num_seeds,
			 const uint8_t *salt, size_t salt_len, const uint8_t *info, size_t info_len);

bool derive_attkb_enc_key(uint8_t *out_key);

#endif /* SEED_H_ */
