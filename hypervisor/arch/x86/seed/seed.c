/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <cpu.h>
#include <pgtable.h>
#include <rtl.h>
#include <mmu.h>
#include <sprintf.h>
#include <ept.h>
#include <logmsg.h>
#include <multiboot.h>
#include <crypto_api.h>
#include <seed.h>
#include "seed_abl.h"
#include "seed_sbl.h"

#define BOOTLOADER_SBL  0U
#define BOOTLOADER_ABL  1U
#define BOOTLOADER_INVD (~0U)

struct seed_argument {
	const char *str;
	uint32_t bootloader_id;
	uint64_t addr;
};

#define SEED_ARG_NUM 4U
static struct seed_argument seed_arg[SEED_ARG_NUM] = {
	{ "ImageBootParamsAddr=",        BOOTLOADER_SBL, 0UL },
	{ "ABL.svnseed=",                BOOTLOADER_ABL, 0UL },
	{ "dev_sec_info.param_addr=",    BOOTLOADER_ABL, 0UL },
	{ NULL, BOOTLOADER_INVD, 0UL }
};

static struct physical_seed g_phy_seed;

static uint32_t parse_seed_arg(void)
{
	const char *cmd_src = NULL;
	char *arg, *arg_end;
	struct acrn_multiboot_info *mbi = get_multiboot_info();
	uint32_t i = SEED_ARG_NUM - 1U;
	uint32_t len;

	if ((mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) != 0U) {
		cmd_src = mbi->mi_cmdline;
	}

	if (cmd_src != NULL) {
		for (i = 0U; seed_arg[i].str != NULL; i++) {
			len = strnlen_s(seed_arg[i].str, MEM_1K);
			arg = strstr_s((const char *)cmd_src, MAX_BOOTARGS_SIZE, seed_arg[i].str, len);
			if (arg != NULL) {
				arg += len;
				seed_arg[i].addr = strtoul_hex(arg);

				/*
				 * Replace original arguments with spaces since Guest's GPA might not
				 * identity mapped to HPA. The argument will be appended later when
				 * compose cmdline for Guest.
				 */
				arg_end = strchr(arg, ' ');
				arg -= len;
				len = (arg_end != NULL) ? (uint32_t)(arg_end - arg) :
					strnlen_s(arg, MAX_BOOTARGS_SIZE);
				(void)memset((void *)arg, (uint8_t)' ', len);
				break;
			}
		}
	}

	return i;
}

/*
 * fill_seed_arg
 *
 * description:
 *     fill seed argument to cmdline buffer which has MAX size of MAX_SEED_ARG_SIZE
 *
 * input:
 *    cmd_dst   pointer to cmdline buffer
 *    cmd_sz    size of cmd_dst buffer
 *
 * output:
 *    cmd_dst   pointer to cmdline buffer
 *
 * return value:
 *    none
 *
 * @pre cmd_dst != NULL
 */
void fill_seed_arg(char *cmd_dst, size_t cmd_sz)
{
	uint32_t i;

	for (i = 0U; seed_arg[i].str != NULL; i++) {
		if (seed_arg[i].addr != 0UL) {

			snprintf(cmd_dst, cmd_sz, "%s0x%X ", seed_arg[i].str, sos_vm_hpa2gpa(seed_arg[i].addr));

			if (seed_arg[i].bootloader_id == BOOTLOADER_SBL) {
				struct image_boot_params *boot_params =
					(struct image_boot_params *)hpa2hva(seed_arg[i].addr);

				boot_params->p_seed_list = sos_vm_hpa2gpa(boot_params->p_seed_list);

				boot_params->p_platform_info = sos_vm_hpa2gpa(boot_params->p_platform_info);
			}

			break;
		}
	}
}

/*
 * derive_virtual_seed
 *
 * description:
 *     derive virtual seed list from physical seed list
 *
 * input:
 *    salt        pointer to salt
 *    salt_len    length of salt
 *    info        pointer to info
 *    info_len    length of info
 *
 * output:
 *    seed_list   pointer to seed_list
 *    num_seed    seed number in seed_list
 *
 * return value:
 *    true if derive successfully, otherwise false
 */
bool derive_virtual_seed(struct seed_info *seed_list, uint32_t *num_seeds,
			 const uint8_t *salt, size_t salt_len, const uint8_t *info, size_t info_len)
{
	uint32_t i;
	bool ret = true;

	if ((seed_list == NULL) || (g_phy_seed.num_seeds == 0U)) {
		ret = false;
	} else {
		for (i = 0U; i < g_phy_seed.num_seeds; i++) {
			if (hkdf_sha256(seed_list[i].seed,
					sizeof(seed_list[i].seed),
					g_phy_seed.seed_list[i].seed,
					sizeof(g_phy_seed.seed_list[i].seed),
					salt, salt_len,
					info, info_len) == 0) {
				*num_seeds = 0U;
				(void)memset(seed_list, 0U, sizeof(struct seed_info) * BOOTLOADER_SEED_MAX_ENTRIES);
				pr_err("%s: derive virtual seed list failed!", __func__);
				ret = false;
				break;
			}
			seed_list[i].cse_svn = g_phy_seed.seed_list[i].cse_svn;
		}
		*num_seeds = g_phy_seed.num_seeds;
	}

	return ret;
}

static inline uint32_t get_max_svn_index(void)
{
	uint32_t i, max_svn_idx = 0U;

	for (i = 1U; i < g_phy_seed.num_seeds; i++) {
		if (g_phy_seed.seed_list[i].cse_svn > g_phy_seed.seed_list[i - 1U].cse_svn) {
			max_svn_idx = i;
		}
	}

	return max_svn_idx;
}

/*
 * derive_attkb_enc_key
 *
 * description:
 *     derive attestation keybox encryption key from physical seed(max svn)
 *
 * input:
 *    none
 *
 * output:
 *    out_key     pointer to output key
 *
 * return value:
 *    true if derive successfully, otherwise false
 */
bool derive_attkb_enc_key(uint8_t *out_key)
{
	bool ret = true;
	const uint8_t *ikm;
	uint32_t ikm_len;
	uint32_t max_svn_idx;
	const uint8_t salt[] = "Attestation Keybox Encryption Key";

	if ((out_key == NULL) || (g_phy_seed.num_seeds == 0U) ||
	    (g_phy_seed.num_seeds > BOOTLOADER_SEED_MAX_ENTRIES)) {
		ret = false;
	} else {
		max_svn_idx = get_max_svn_index();
		ikm = &(g_phy_seed.seed_list[max_svn_idx].seed[0]);
		/* only the low 32 bytes of seed are valid */
		ikm_len = 32U;

		if (hmac_sha256(out_key, ikm, ikm_len, salt, sizeof(salt)) != 1) {
			pr_err("%s: failed to derive key!\n", __func__);
			ret = false;
		}
	}

	return ret;
}

void init_seed(void)
{
	bool status;
	uint32_t index;

	index = parse_seed_arg();

	switch (seed_arg[index].bootloader_id) {
	case BOOTLOADER_SBL:
		status = parse_seed_sbl(seed_arg[index].addr, &g_phy_seed);
		break;
	case BOOTLOADER_ABL:
		status = parse_seed_abl(seed_arg[index].addr, &g_phy_seed);
		break;
	default:
		status = false;
		break;
	}

	/* Failed to parse seed from Bootloader, using dummy seed */
	if (!status) {
		g_phy_seed.num_seeds = 1U;
		(void)memset(&g_phy_seed.seed_list[0], 0xA5U, sizeof(g_phy_seed.seed_list));
	}
}
