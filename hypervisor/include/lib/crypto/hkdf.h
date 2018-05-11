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
		const uint8_t *salt, size_t salt_len,
		const uint8_t *info, size_t info_len);

#endif  /* HKDF_H */
