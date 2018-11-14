/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file crypto_api.h
 *
 * @brief public APIs for crypto functions
 */

#ifndef CRYPTO_API_H
#define CRYPTO_API_H

#include <types.h>

/**
 * @brief HMAC-based Extract-and-Expand Key Derivation Function.
 *
 * @param   out_key    Pointer to key buffer which is used to save
 *                     hkdf_sha256 result
 * @param   out_len    The length of out_key
 * @param   secret     Pointer to input keying material
 * @param   secret_len The length of secret
 * @param   salt       Pointer to salt buffer, it is optional
 *                  if not provided (salt == NULL), it is set internally
 *                  to a string of hashlen(32) zeros
 * @param   salt_len   The length of the salt value
 *                     Ignored if salt is NULL
 * @param   info       Pointer to application specific information, it is
 *                     optional. Ignored if info == NULL or a zero-length string
 * @param   info_len:  The length of the info, ignored if info is NULL
 *
 * @return int 1 - Success  0 - Failure
 */
int hkdf_sha256(uint8_t *out_key, size_t out_len,
		const uint8_t *secret, size_t secret_len,
		const uint8_t *salt, size_t salt_len,
		const uint8_t *info, size_t info_len);

/**
 * @brief This function calculates the full generic HMAC
 *      on the input buffer with the provided key.
 *
 *      The function allocates the context, performs the
 *      calculation, and frees the context.
 *
 *      The HMAC result is calculated as
 *      output = generic HMAC(hmac key, input buffer).
 *
 * @param   out_key     The generic HMAC result
 * @param   secret      The HMAC secret key
 * @param   secret_len  The length of the HMAC secret key in Bytes
 * @param   salt        The buffer holding the input data
 * @param   salt_len    The length of the input data
 *
 * @return int 1 - Success  0 - Failure
 */
int hmac_sha256(uint8_t *out_key,
		const uint8_t *secret, size_t secret_len,
		const uint8_t *salt, size_t salt_len);

#endif  /* CRYPTO_API_H */
