/*
 *  HKDF implementation -- RFC 5869
 *
 *  Copyright (C) 2016-2018, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#include "hkdf.h"

int32_t mbedtls_hkdf(const mbedtls_md_info_t *md, const uint8_t *salt,
                  size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                  const uint8_t *info, size_t info_len,
                  uint8_t *okm, size_t okm_len)
{
    int32_t ret;
    uint8_t prk[MBEDTLS_MD_MAX_SIZE];

    ret = mbedtls_hkdf_extract(md, salt, salt_len, ikm, ikm_len, prk);

    if (ret == 0) {
        ret = mbedtls_hkdf_expand(md, prk, (size_t)mbedtls_md_get_size(md),
                                   info, info_len, okm, okm_len);
    }

    (void)mbedtls_platform_zeroize(prk, sizeof(prk));

    return ret;
}

int32_t mbedtls_hkdf_extract(const mbedtls_md_info_t *md,
                          const uint8_t *salt, size_t salt_len,
                          const uint8_t *ikm, size_t ikm_len,
                          uint8_t *prk)
{
    int32_t ret = 0;
    size_t tmp_salt_len = salt_len;
    const uint8_t *tmp_salt = salt;
    uint8_t null_salt[MBEDTLS_MD_MAX_SIZE] = { 0U };

    if (tmp_salt == NULL) {
        size_t hash_len;

        if (tmp_salt_len != 0U) {
            ret =  MBEDTLS_ERR_HKDF_BAD_INPUT_DATA;
        } else {

            hash_len = mbedtls_md_get_size(md);

            if (hash_len == 0U) {
                ret = MBEDTLS_ERR_HKDF_BAD_INPUT_DATA;
            } else {
                tmp_salt = null_salt;
                tmp_salt_len = hash_len;
            }
        }
    }

    if (ret == 0) {
        ret = mbedtls_md_hmac(md, tmp_salt, tmp_salt_len, ikm, ikm_len, prk);
    }

    return ret;
}

int32_t mbedtls_hkdf_expand(const mbedtls_md_info_t *md, const uint8_t *prk,
                         size_t prk_len, const uint8_t *info,
                         size_t info_len, uint8_t *okm, size_t okm_len)
{
    size_t hash_len;
    size_t where = 0;
    size_t n;
    size_t t_len = 0;
    size_t tmp_info_len = info_len;
    const uint8_t *tmp_info = info;
    size_t i;
    int32_t ret = 0;
    mbedtls_md_context_t ctx;
    uint8_t t[MBEDTLS_MD_MAX_SIZE];

    hash_len = mbedtls_md_get_size(md);

    if ((okm == NULL) || (prk_len < hash_len) || (hash_len == 0U)) {
        ret = MBEDTLS_ERR_HKDF_BAD_INPUT_DATA;
    } else {

        if (tmp_info == NULL) {
            tmp_info = (const uint8_t *) "";
            tmp_info_len = 0U;
        }

        n = okm_len / hash_len;

        if ((okm_len % hash_len) != 0U) {
            n++;
        }

        /*
         * Per RFC 5869 Section 2.3, okm_len must not exceed
         * 255 times the hash length
         */
        if (n > 255U) {
            ret = MBEDTLS_ERR_HKDF_BAD_INPUT_DATA;
        } else {
            mbedtls_md_init(&ctx);

            ret = mbedtls_md_setup(&ctx, md);
            if (ret == 0) {

                /*
                 * Compute T = T(1) | T(2) | T(3) | ... | T(N)
                 * Where T(N) is defined in RFC 5869 Section 2.3
                 */
                for (i = 1U; i <= n; i++) {
                    size_t num_to_copy;
                    uint8_t c = (uint8_t)(i & 0xffU);

                    ret = mbedtls_md_hmac_starts(&ctx, prk, prk_len);
                    if (ret == 0) {
                        ret = mbedtls_md_hmac_update(&ctx, t, t_len);
                    }

                    if (ret == 0) {
                        ret = mbedtls_md_hmac_update(&ctx, tmp_info, tmp_info_len);
                    }

                    /* The constant concatenated to the end of each T(n) is a single octet.
                     * */
                    if (ret == 0) {
                        ret = mbedtls_md_hmac_update(&ctx, &c, 1);
                    }

                    if (ret == 0) {
                        ret = mbedtls_md_hmac_finish(&ctx, t);
                    }

                    if (ret != 0) {
                        break;
                    }

                    num_to_copy = (i != n) ? hash_len : (okm_len - where);
                    (void)memcpy_s(okm + where, num_to_copy, t, num_to_copy);
                    where += hash_len;
                    t_len = hash_len;
                }
            }

             mbedtls_md_free(&ctx);
        }
    }

    (void)mbedtls_platform_zeroize(t, sizeof(t));

    return ret;
}
