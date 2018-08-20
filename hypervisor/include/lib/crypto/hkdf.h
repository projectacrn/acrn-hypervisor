/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HKDF_H
#define HKDF_H

#include <types.h>

/*
 *  FUNCTION
 *      hkdf_sha256
 *
 *  Description
 *      HMAC-based Extract-and-Expand Key Derivation Function.
 *
 *  Parameters:
 *      out_key     Pointer to key buffer which is used to save
 *                  hkdf_sha256 result
 *      out_len     The length of out_key
 *      secret      Pointer to input keying material
 *      secret_len  The length of secret
 *      salt        Pointer to salt buffer, it is optional
 *                  if not provided (salt == NULL), it is set internally
 *                  to a string of hashlen(32) zeros
 *      salt_len    The length of the salt value
 *                  Ignored if salt is NULL
 *      info        Pointer to application specific information, it is
 *                  optional
 *                  Ignored if info == NULL or a zero-length string
 *      info_len:   The length of the info, ignored if info is NULL
 *
 *  OUTPUTS
 *      1 - Success
 *      0 - Failure
 */
int hkdf_sha256(uint8_t *out_key, size_t out_len,
		const uint8_t *secret, size_t secret_len,
		__unused const uint8_t *salt, __unused size_t salt_len,
		__unused const uint8_t *info, __unused size_t info_len);

#endif  /* HKDF_H */
