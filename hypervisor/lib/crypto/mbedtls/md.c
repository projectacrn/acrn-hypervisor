/**
 * \file mbedtls_md.c
 *
 * \brief Generic message digest wrapper for mbed TLS
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Copyright (C) 2018, Intel Corporation, All Rights Reserved.
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

#include <hypervisor.h>
#include "md.h"
#include "md_internal.h"

/*
 * Reminder: update profiles in x509_crt.c when adding a new hash!
 */

const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t md_type)
{
    const mbedtls_md_info_t *md_info;

    switch (md_type)
    {
        case MBEDTLS_MD_SHA256:
            md_info = &mbedtls_sha256_info;
            break;
        default:
            md_info = NULL;
            break;
    }

    return md_info;
}

void mbedtls_md_init(mbedtls_md_context_t *ctx)
{
    (void) memset(ctx, 0U, sizeof(mbedtls_md_context_t));
}

void mbedtls_md_free(mbedtls_md_context_t *ctx)
{
    if (ctx != NULL) {
        (void) mbedtls_platform_zeroize(ctx, sizeof(mbedtls_md_context_t));
    }

    return;
}

int32_t mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *md_info)
{
    int32_t ret = 0;

    if ((md_info == NULL) || (ctx == NULL)) {
        ret = MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    } else {
        ctx->md_info = md_info;
    }

    return ret;
}

int32_t mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const uint8_t *key, size_t keylen)
{
    int32_t ret = 0;
    uint8_t sum[MBEDTLS_MD_MAX_SIZE];
    uint8_t *ipad, *opad;
    const uint8_t *temp_key = key;
    size_t i;

    if ((ctx == NULL) || (ctx->md_info == NULL) || (ctx->hmac_ctx == NULL) || (temp_key == NULL)) {
        ret = MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if (ret == 0) {
        if (keylen > ctx->md_info->block_size) {
            ret = ctx->md_info->starts_func((void *) ctx->md_ctx);
            if (ret == 0) {
                ret = ctx->md_info->update_func((void *) ctx->md_ctx, temp_key, keylen);
                if (ret == 0) {
                    ret = ctx->md_info->finish_func((void *) ctx->md_ctx, sum);
                }
            }

            if (ret == 0) {
                keylen = (size_t) ctx->md_info->size;
                temp_key = sum;
            }
        }

        if (ret == 0) {
            ipad = (uint8_t *) ctx->hmac_ctx;
            opad = (uint8_t *) ctx->hmac_ctx + ctx->md_info->block_size;

            (void) memset(ipad, 0x36U, ctx->md_info->block_size);
            (void) memset(opad, 0x5CU, ctx->md_info->block_size);

            for (i = 0U; i < keylen; i++) {
                *(ipad + i) = (uint8_t) (*(ipad + i) ^ (*(temp_key + i)));
                *(opad + i) = (uint8_t) (*(opad + i) ^ (*(temp_key + i)));
            }

            ret = ctx->md_info->starts_func((void *) ctx->md_ctx);
            if (ret == 0) {
                ret = ctx->md_info->update_func((void *) ctx->md_ctx, ipad,
                                                   ctx->md_info->block_size);
            }
        }
        (void) mbedtls_platform_zeroize(sum, sizeof(sum));
    }

    return ret;
}

int32_t mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const uint8_t *input, size_t ilen)
{
    int32_t ret;

    if ((ctx == NULL) || (ctx->md_info == NULL) || (ctx->hmac_ctx == NULL)) {
        ret = MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    } else {
        ret = ctx->md_info->update_func((void *) ctx->md_ctx, input, ilen);
    }

    return ret;
}

int32_t mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, uint8_t *output)
{
    int32_t ret = 0;
    uint8_t tmp[MBEDTLS_MD_MAX_SIZE];
    uint8_t *opad;

    if ((ctx == NULL) || (ctx->md_info == NULL) || (ctx->hmac_ctx == NULL)) {
        ret = MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if (ret == 0) {
        opad = (uint8_t *) ctx->hmac_ctx + ctx->md_info->block_size;

        ret = ctx->md_info->finish_func((void *) ctx->md_ctx, tmp);
        if (ret == 0) {
            ret = ctx->md_info->starts_func((void *) ctx->md_ctx);
        }
    }

    if (ret == 0) {
        ret = ctx->md_info->update_func((void *) ctx->md_ctx, opad,
                                         ctx->md_info->block_size);
        if (ret == 0) {
            ret = ctx->md_info->update_func((void *) ctx->md_ctx, tmp,
                                             ctx->md_info->size);
        }

        if (ret == 0) {
            ret = ctx->md_info->finish_func((void *) ctx->md_ctx,
                                             (uint8_t *) output);
        }
    }

    return ret;
}

int32_t mbedtls_md_hmac(const mbedtls_md_info_t *md_info,
                     const uint8_t *key, size_t keylen,
                     const uint8_t *input, size_t ilen,
                     uint8_t *output)
{
    mbedtls_md_context_t ctx;
    int32_t ret = 0;

    if (md_info == NULL) {
        ret = MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    if (ret == 0) {
        mbedtls_md_init(&ctx);

        ret = mbedtls_md_setup(&ctx, md_info);
        if (ret == 0) {
            ret = mbedtls_md_hmac_starts(&ctx, key, keylen);
        }

        if (ret == 0) {
            ret = mbedtls_md_hmac_update(&ctx, input, ilen);
        }

        if (ret == 0) {
            ret = mbedtls_md_hmac_finish(&ctx, output);
        }

        mbedtls_md_free(&ctx);
    }

    return ret;
}

uint8_t mbedtls_md_get_size(const mbedtls_md_info_t *md_info)
{
    uint8_t ret = 0U;

    if (md_info != NULL) {
        ret = (uint8_t) md_info->size;
    }

    return ret;
}
