/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rtl.h>

#include "tinycrypt/hmac.h"
#include "tinycrypt/sha256.h"

#define SHA256_HASH_SIZE 32 /* SHA-256 length */

static uint8_t *hmac_sha256(uint8_t *out, unsigned int out_len,
				const void *key, size_t key_len,
				const uint8_t *data, size_t data_len)
{
	struct tc_hmac_state h;

	memset(&h, 0x0, sizeof(h));

	if (!tc_hmac_set_key(&h, key, key_len) ||
			!tc_hmac_init(&h) ||
			!tc_hmac_update(&h, data, data_len) ||
			!tc_hmac_final(out, out_len, &h)) {
		out = NULL;
	}

	memset(&h, 0x0, sizeof(h));

	return out;
}

/* This function implements HKDF extract
 * https://tools.ietf.org/html/rfc5869#section-2.2
 */
static int hkdf_sha256_extract(uint8_t *out_key, size_t out_len,
				const uint8_t *secret, size_t secret_len,
				const uint8_t *salt, size_t salt_len)
{
	uint8_t salt0[SHA256_HASH_SIZE];

	/* salt is optional for hkdf_sha256, it can be NULL.
	 *  The implement of tc_hmac_set_key in tinycrypt can't
	 *  accept NULL pointer, so salt0 is used here and set
	 *  to all 0s
	 */
	if (!salt || salt_len == 0) {
		memset(salt0, 0, SHA256_HASH_SIZE);
		salt = salt0;
		salt_len = SHA256_HASH_SIZE;
	}

	if (!hmac_sha256(out_key, out_len,
				salt, salt_len,
				secret, secret_len))
		return 0;

	return 1;
}

/* This function implements HKDF expand
 * https://tools.ietf.org/html/rfc5869#section-2.3
 */
static int hkdf_sha256_expand(uint8_t *out_key, size_t out_len,
				const uint8_t *prk, size_t prk_len,
				const uint8_t *info, size_t info_len)
{
	const size_t digest_len = SHA256_HASH_SIZE;
	uint8_t T[SHA256_HASH_SIZE];
	size_t n, done = 0;
	unsigned int i;
	int ret = 0;
	struct tc_hmac_state h;

	n = (out_len + digest_len - 1) / digest_len;
	if (n > 255)
		return 0;

	memset(&h, 0x0, sizeof(h));

	for (i = 0; i < n; i++) {
		uint8_t ctr = i + 1;
		size_t todo;

		tc_hmac_set_key(&h, prk, prk_len);
		tc_hmac_init(&h);
		if (i != 0 && (!tc_hmac_update(&h, T, digest_len)))
			goto out;

		if (!tc_hmac_update(&h, info, info_len) ||
				!tc_hmac_update(&h, &ctr, 1) ||
				!tc_hmac_final(T, digest_len, &h)) {
			goto out;
		}

		todo = digest_len;
		/* Check if the length of left buffer is smaller than
		 * 32 to make sure no buffer overflow in below memcpy
		 */
		if (done + todo > out_len)
			todo = out_len - done;

		memcpy_s(out_key + done, todo, T, todo);
		done += todo;
	}

	ret = 1;

out:
	memset(&h, 0x0, sizeof(h));
	memset(T, 0x0, SHA256_HASH_SIZE);

	return ret;
}

/* https://tools.ietf.org/html/rfc5869#section-2 */
int hkdf_sha256(uint8_t *out_key, size_t out_len,
		const uint8_t *secret, size_t secret_len,
		const uint8_t *salt, size_t salt_len,
		const uint8_t *info, size_t info_len)
{
	uint8_t prk[SHA256_HASH_SIZE];
	size_t prk_len = SHA256_HASH_SIZE;

	if (!hkdf_sha256_extract(prk, prk_len,
				secret, secret_len,
				salt, salt_len)) {
		return 0;
	}

	if (!hkdf_sha256_expand(out_key, out_len,
				prk, prk_len,
				info, info_len)) {
		return 0;
	}

	return 1;
}
