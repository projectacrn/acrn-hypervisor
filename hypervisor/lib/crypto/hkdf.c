/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <rtl.h>
#include <hkdf.h>

int hkdf_sha256(uint8_t *out_key, size_t out_len,
		const uint8_t *secret, size_t secret_len,
		__unused const uint8_t *salt, __unused size_t salt_len,
		__unused const uint8_t *info, __unused size_t info_len)
{
	/* FIXME: currently, we only support one AaaG/Trusty
	 * instance, so just simply copy the h/w seed to Trusty.
	 * In the future, we will choose another crypto library
	 * to derive multiple seeds in order to support multiple
	 * AaaG/Trusty instances.
	 */
	(void)memcpy_s(out_key, out_len, secret, secret_len);

	return 1;
}
