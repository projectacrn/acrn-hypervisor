/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <abl_seed_parse.h>

#define ABL_SEED_LEN 32U
struct abl_seed_info {
	uint8_t svn;
	uint8_t reserved[3];
	uint8_t seed[ABL_SEED_LEN];
};

#define ABL_SEED_LIST_MAX 4U
struct dev_sec_info {
	uint32_t size_of_this_struct;
	uint32_t version;
	uint32_t num_seeds;
	struct abl_seed_info seed_list[ABL_SEED_LIST_MAX];
};

static const char *boot_params_arg = "dev_sec_info.param_addr=";

static void parse_seed_list_abl(void *param_addr)
{
	uint32_t i;
	uint32_t legacy_seed_index = 0;
	uint32_t svn_seed_index;
	struct seed_info dseed_list[BOOTLOADER_SEED_MAX_ENTRIES];
	struct dev_sec_info *sec_info = (struct dev_sec_info *)param_addr;

	if (sec_info == NULL)
		goto fail;

	(void)memset(dseed_list, 0U, sizeof(dseed_list));
	for (i = 0U; i < sec_info->num_seeds; i++) {
		dseed_list[i].cse_svn = sec_info->seed_list[i].svn;
		(void)memcpy_s(dseed_list[i].seed,
				sizeof(dseed_list[i].seed),
				sec_info->seed_list[i].seed,
				sizeof(sec_info->seed_list[i].seed));

		if (i == 0)
			continue;

		/*
		 * The seed_list from ABL contains several seeds which based on
		 * SVN and one legacy seed which is not based on SVN. The
		 * legacy seed's svn is the minimum in the seed list. And CSE
		 * ensures at least two seeds will be generated which will
		 * contain the legacy seed.
		 * The SOS requires the legacy seed for RPMB usage. So here find
		 * and erase the SVN based seed only.
		 */
		if (sec_info->seed_list[i].svn > sec_info->seed_list[legacy_seed_index].svn) {
			svn_seed_index = i;
		} else {
			svn_seed_index = legacy_seed_index;
			legacy_seed_index = i;
		}
		(void)memset(sec_info->seed_list[svn_seed_index].seed, 0U,
				sizeof(sec_info->seed_list[svn_seed_index].seed));
	}


	trusty_set_dseed(dseed_list, sec_info->num_seeds);
	(void)memset(dseed_list, 0U, sizeof(dseed_list));
	return;
fail:
	trusty_set_dseed(NULL, 0U);
	(void)memset(dseed_list, 0U, sizeof(dseed_list));
}

bool abl_seed_parse(struct vm *vm, char *cmdline, char *out_arg, uint32_t out_len)
{
	char *arg, *arg_end;
	char *param;
	void *param_addr;
	uint32_t len;

	if (cmdline == NULL) {
		goto fail;
	}

	len = strnlen_s(boot_params_arg, MEM_1K);
	arg = strstr_s(cmdline, MEM_2K, boot_params_arg, len);

	if (arg == NULL) {
		goto fail;
	}

	param = arg + len;
	param_addr = (void *)hpa2hva(strtoul_hex(param));
	if (param_addr == NULL) {
		goto fail;
	}

	parse_seed_list_abl(param_addr);

	/*
	 * Replace original arguments with spaces since SOS's GPA is not
	 * identity mapped to HPA. The argument will be appended later when
	 * compose cmdline for SOS.
	 */
	arg_end = strchr(arg, ' ');
	len = (arg_end != NULL) ? (uint32_t)(arg_end - arg) :
							strnlen_s(arg, MEM_2K);
	(void)memset(arg, ' ', len);

	/* Convert the param_addr to SOS GPA and copy to caller */
	if (out_arg) {
		snprintf(out_arg, out_len, "%s0x%X ",
			boot_params_arg, hva2gpa(vm, param_addr));
	}

	return true;

fail:
	parse_seed_list_abl(NULL);
	return false;
}
