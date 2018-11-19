/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rtl.h>
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"

int hkdf_sha256(uint8_t *out_key, size_t out_len,
		const uint8_t *secret, size_t secret_len,
		const uint8_t *salt, size_t salt_len,
		const uint8_t *info, size_t info_len)
{
	const mbedtls_md_info_t *md;

	/* salt and info can be NULL, others can't */
	if (!out_key || !secret) {
		return 0;
	}

	md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	if (md == NULL) {
		return 0;
	}

	if (mbedtls_hkdf(md,
			salt, salt_len,
			secret, secret_len,
			info, info_len,
			out_key, out_len) != 0) {
		return 0;
	}

	return 1;
}

int hmac_sha256(uint8_t *out_key,
		const uint8_t *secret, size_t secret_len,
		const uint8_t *salt, size_t salt_len)
{
	const mbedtls_md_info_t *md;

	if (!out_key || !secret || !salt) {
		return 0;
	}

	md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
	if (md == NULL) {
		return 0;
	}

	if (mbedtls_md_hmac(md,
			secret, secret_len,
			salt, salt_len,
			out_key) != 0) {
		return 0;
	}

	return 1;
}
