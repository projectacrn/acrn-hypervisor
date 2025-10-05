/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Bare boot mode is useful when we run on platforms that
 * does not support multiboot. Bare boot allows you to pre-configure
 * ACRN boot components in compile time.
 */

#include <types.h>
#include <boot.h>
#include <bare.h>
#include <rtl.h>
#include <pgtable.h>
#include <logmsg.h>

static struct bare_boot_option *options;
static uint16_t nmods;

uint16_t get_mod_count()
{
	return nmods;
}

void *get_mod_addr(uint16_t mod_idx)
{
	void *ret = NULL;
	if (mod_idx < nmods) {
		ret = hpa2hva_early(options[mod_idx].addr);
	}
	return ret;
}

uint64_t get_mod_size(uint16_t mod_idx)
{
	uint64_t ret = 0;
	if (mod_idx < nmods) {
		ret = options[mod_idx].size;
	}
	return ret;
}

const char *get_mod_tag(uint16_t mod_idx)
{
	const char *ret = NULL;
	if (mod_idx < nmods) {
		ret = options[mod_idx].tag;
	}
	return ret;
}

int32_t init_bare_boot_info()
{
	extern struct bare_boot_option bare_boot_options[];
	extern uint16_t n_bare_boot_options;
	struct acrn_boot_info *abi = get_acrn_boot_info();
	struct abi_module *m;
	const char *tag;
	int i;

	(void)strncpy_s((void *)abi->protocol_name, MAX_PROTOCOL_NAME_SIZE,
			"Bare boot", (MAX_PROTOCOL_NAME_SIZE - 1U));

	(void)strncpy_s((void *)(abi->loader_name), MAX_LOADER_NAME_SIZE,
			"Bare boot Loader", (MAX_LOADER_NAME_SIZE - 1U));

	options = bare_boot_options;
	nmods = n_bare_boot_options;

	if (nmods > MAX_MODULE_NUM) {
		pr_err("Bareboot: Too many boot modules (%d found)", nmods);
		pr_err("Bareboot: Accepting only %d, ignoring rest", MAX_MODULE_NUM);
		nmods = MAX_MODULE_NUM;
	}

	abi->mods_count = nmods;

	for (i = 0; i < nmods; i++) {
		m = &(abi->mods[i]);
		m->start = get_mod_addr(i);
		m->size = get_mod_size(i);
		tag = get_mod_tag(i);
		(void)strncpy_s((void *)(m->string), MAX_MOD_STRING_SIZE,
				tag, strnlen_s(tag, MAX_MOD_STRING_SIZE));
	}

	return 0;
}
