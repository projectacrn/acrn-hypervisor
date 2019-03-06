/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <sprintf.h>
#include <vm.h>
#include <mmu.h>
#include <logmsg.h>
#include <sbl_seed_parse.h>
#include <ept.h>

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

struct image_boot_params {
	uint32_t size_of_this_struct;
	uint32_t version;
	uint64_t p_seed_list;
	uint64_t p_platform_info;
	uint64_t reserved;
};

static const char *boot_params_arg = "ImageBootParamsAddr=";

static void parse_seed_list_sbl(struct seed_list_hob *seed_hob)
{
	uint8_t i;
	uint8_t dseed_index = 0U;
	struct seed_entry *entry;
	struct seed_info dseed_list[BOOTLOADER_SEED_MAX_ENTRIES];
	bool parse_success = false;

	if ((seed_hob != NULL) && (seed_hob->total_seed_count != 0U)) {
		parse_success = true;

		entry = (struct seed_entry *)((uint8_t *)seed_hob + sizeof(struct seed_list_hob));

		for (i = 0U; i < seed_hob->total_seed_count; i++) {
			if (entry == NULL) {
				break;
			}
			/* retrieve dseed */
			if ((SEED_ENTRY_TYPE_SVNSEED == entry->type) &&	(SEED_ENTRY_USAGE_DSEED == entry->usage)) {

				/* The seed_entry with same type/usage are always
				 * arranged by index in order of 0~3.
				 */
				if (entry->index != dseed_index) {
					pr_warn("Index mismatch. Use fake seed!");
					parse_success = false;
					break;
				}

				if (entry->index >= BOOTLOADER_SEED_MAX_ENTRIES) {
					pr_warn("Index exceed max number!");
					parse_success = false;
					break;
				}

				(void)memcpy_s((void *)&dseed_list[dseed_index], sizeof(struct seed_info),
						(void *)entry->seed, sizeof(struct seed_info));
				dseed_index++;

				/* erase original seed in seed entry */
				(void)memset((void *)entry->seed, 0U, sizeof(struct seed_info));
			}

			entry = (struct seed_entry *)((uint8_t *)entry + entry->seed_entry_size);
		}
	}

	if (parse_success) {
		trusty_set_dseed((void *)dseed_list, dseed_index);
	} else {
		trusty_set_dseed(NULL, 0U);
	}

	(void)memset((void *)dseed_list, 0U, sizeof(dseed_list));
}

/*
 * sbl_seed_parse
 *
 * description:
 *    This function parse image_boot_params from cmdline. Mainly on
 *    1. parse structure address from cmdline
 *    2. get seed_list address and call parse_seed_list_sbl to parse seed
 *    3. convert address in the structure from HPA to SOS's GPA
 *    4. clear original image_boot_params argument in cmdline since
 *       original address is HPA.
 *
 * input:
 *    vm_is_sos     Boolean to check if vm is sos
 *    cmdline       pointer to cmdline string
 *    out_len       the max len of out_arg
 *
 * output:
 *    out_arg       the argument with SOS's GPA
 *
 * return value:
 *    true if parse successfully, otherwise false.
 */
bool sbl_seed_parse(bool vm_is_sos, char *cmdline, char *out_arg, uint32_t out_len)
{
	char *arg, *arg_end;
	char *param;
	void *param_addr;
	struct image_boot_params *boot_params;
	uint32_t len;
	bool parse_success = false;

	if (vm_is_sos && (cmdline != NULL)) {
		len = strnlen_s(boot_params_arg, MEM_1K);
		arg = strstr_s((const char *)cmdline, MEM_2K, boot_params_arg, len);

		if (arg != NULL) {
			param = arg + len;
			param_addr = (void *)hpa2hva(strtoul_hex(param));
			if (param_addr != NULL) {
				boot_params = (struct image_boot_params *)param_addr;
				parse_seed_list_sbl((struct seed_list_hob *)hpa2hva(boot_params->p_seed_list));

				/*
				 * Convert the addresses to SOS GPA since this structure will
				 * be used in SOS.
				 */
				boot_params->p_seed_list = sos_vm_hpa2gpa(boot_params->p_seed_list);
				boot_params->p_platform_info = sos_vm_hpa2gpa(boot_params->p_platform_info);

				/*
				 * Replace original arguments with spaces since SOS's GPA is not
				 * identity mapped to HPA. The argument will be appended later when
				 * compose cmdline for SOS.
				 */
				arg_end = strchr(arg, ' ');
				len = (arg_end != NULL) ? (uint32_t)(arg_end - arg) : strnlen_s(arg, MEM_2K);
				(void)memset((void *)arg, ' ', len);

				/* Convert the param_addr to SOS GPA and copy to caller */
				if (out_arg != NULL) {
					snprintf(out_arg, out_len, "%s0x%X ",
						boot_params_arg, sos_vm_hpa2gpa(hva2hpa(param_addr)));
				}

				parse_success = true;
			}
		}
	}

	if (!parse_success) {
		parse_seed_list_sbl(NULL);
	}

	return parse_success;
}
