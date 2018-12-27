/*
 *  FIPS-180-2 compliant SHA-256 implementation
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
/*
 *  The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#include "md.h"
#include "sha256.h"

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                            \
do {                                                    \
    (n) = ((uint32_t) (b)[(i)    ] << 24)             \
        | ((uint32_t) (b)[(i) + 1] << 16)             \
        | ((uint32_t) (b)[(i) + 2] <<  8)             \
        | ((uint32_t) (b)[(i) + 3]      );            \
} while(0)
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                            \
do {                                                    \
    (b)[(i)    ] = (uint8_t) ((n) >> 24);       \
    (b)[(i) + 1] = (uint8_t) ((n) >> 16);       \
    (b)[(i) + 2] = (uint8_t) ((n) >>  8);       \
    (b)[(i) + 3] = (uint8_t) ((n)      );       \
} while(0)
#endif

void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    memset(ctx, 0U, sizeof(mbedtls_sha256_context));
}

void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    if (ctx != NULL) {
        mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha256_context));
    }
}

void mbedtls_sha256_clone(mbedtls_sha256_context *dst, const mbedtls_sha256_context *src)
{
    *dst = *src;
}

/*
 * SHA-256 context setup
 */
int32_t mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int32_t is224)
{
    ctx->total[0] = 0U;
    ctx->total[1] = 0U;

    if (is224 == 0) {
        /* SHA-256 */
        ctx->state[0] = 0x6A09E667U;
        ctx->state[1] = 0xBB67AE85U;
        ctx->state[2] = 0x3C6EF372U;
        ctx->state[3] = 0xA54FF53AU;
        ctx->state[4] = 0x510E527FU;
        ctx->state[5] = 0x9B05688CU;
        ctx->state[6] = 0x1F83D9ABU;
        ctx->state[7] = 0x5BE0CD19U;
    } else {
        /* SHA-224 */
        ctx->state[0] = 0xC1059ED8U;
        ctx->state[1] = 0x367CD507U;
        ctx->state[2] = 0x3070DD17U;
        ctx->state[3] = 0xF70E5939U;
        ctx->state[4] = 0xFFC00B31U;
        ctx->state[5] = 0x68581511U;
        ctx->state[6] = 0x64F98FA7U;
        ctx->state[7] = 0xBEFA4FA4U;
    }

    ctx->is224 = is224;

    return(0);
}

static const uint32_t K[] =
{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
};

#define  SHR(x,n) (((x) & 0xFFFFFFFFU) >> (n))
#define ROTR(x,n) (SHR((x),(n)) | ((x) << (32U - (n))))

#define S0(x) (ROTR((x), 7U) ^ ROTR((x),18U) ^  SHR((x), 3U))
#define S1(x) (ROTR((x),17U) ^ ROTR((x),19U) ^  SHR((x),10U))

#define S2(x) (ROTR((x), 2U) ^ ROTR((x),13U) ^ ROTR((x),22U))
#define S3(x) (ROTR((x), 6U) ^ ROTR((x),11U) ^ ROTR((x),25U))

#define F0(x,y,z) (((x) & (y)) | ((z) & ((x) | (y))))
#define F1(x,y,z) ((z) ^ ((x) & ((y) ^ (z))))

#define R(t)                                    \
(                                              \
    W[(t)] = S1(W[(t) -  2]) + W[(t) -  7] +          \
           S0(W[(t) - 15]) + W[(t) - 16]            \
)

#define P(a,b,c,d,e,f,g,h,x,K)                  \
{                                               \
    temp1 = (h) + S3(e) + F1((e),(f),(g)) + (K) + (x);      \
    temp2 = S2(a) + F0((a),(b),(c));                  \
    (d) += temp1; (h) = temp1 + temp2;              \
}

int32_t mbedtls_internal_sha256_process(mbedtls_sha256_context *ctx, const uint8_t data[64])
{
    uint32_t temp1, temp2, W[64];
    uint32_t A[8];
    int32_t i;

    for (i = 0; i < 8; i++) {
        A[i] = ctx->state[i];
    }

    for (i = 0; i < 16; i++) {
        GET_UINT32_BE(W[i], data, 4 * i);
    }

    for (i = 0; i < 16; i += 8) {
        P(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7], W[i+0], K[i+0]);
        P(A[7], A[0], A[1], A[2], A[3], A[4], A[5], A[6], W[i+1], K[i+1]);
        P(A[6], A[7], A[0], A[1], A[2], A[3], A[4], A[5], W[i+2], K[i+2]);
        P(A[5], A[6], A[7], A[0], A[1], A[2], A[3], A[4], W[i+3], K[i+3]);
        P(A[4], A[5], A[6], A[7], A[0], A[1], A[2], A[3], W[i+4], K[i+4]);
        P(A[3], A[4], A[5], A[6], A[7], A[0], A[1], A[2], W[i+5], K[i+5]);
        P(A[2], A[3], A[4], A[5], A[6], A[7], A[0], A[1], W[i+6], K[i+6]);
        P(A[1], A[2], A[3], A[4], A[5], A[6], A[7], A[0], W[i+7], K[i+7]);
    }

    for (i = 16; i < 64; i += 8) {
        P(A[0], A[1], A[2], A[3], A[4], A[5], A[6], A[7], R(i+0), K[i+0]);
        P(A[7], A[0], A[1], A[2], A[3], A[4], A[5], A[6], R(i+1), K[i+1]);
        P(A[6], A[7], A[0], A[1], A[2], A[3], A[4], A[5], R(i+2), K[i+2]);
        P(A[5], A[6], A[7], A[0], A[1], A[2], A[3], A[4], R(i+3), K[i+3]);
        P(A[4], A[5], A[6], A[7], A[0], A[1], A[2], A[3], R(i+4), K[i+4]);
        P(A[3], A[4], A[5], A[6], A[7], A[0], A[1], A[2], R(i+5), K[i+5]);
        P(A[2], A[3], A[4], A[5], A[6], A[7], A[0], A[1], R(i+6), K[i+6]);
        P(A[1], A[2], A[3], A[4], A[5], A[6], A[7], A[0], R(i+7), K[i+7]);
    }

    for (i = 0; i < 8; i++) {
        ctx->state[i] += A[i];
    }

    return 0;
}

/*
 * SHA-256 process buffer
 */
int32_t mbedtls_sha256_update_ret(mbedtls_sha256_context *ctx, const uint8_t *input, size_t ilen)
{
    int32_t ret = 0;
    size_t fill;
    uint32_t left;
    const uint8_t *data = input;
    size_t len = ilen;

    if ((len != 0U) && (data != NULL)) {
        left = ctx->total[0] & 0x3FU;
        fill = 64U - left;

        ctx->total[0] += (uint32_t)len;
        ctx->total[0] &= 0xFFFFFFFFU;

        if (ctx->total[0] < (uint32_t)len) {
            ctx->total[1]++;
        }

        if ((left != 0U) && (len >= fill)) {
            (void)memcpy_s((void *)&ctx->buffer[left], fill, data, fill);

            ret = mbedtls_internal_sha256_process(ctx, ctx->buffer);
            if (ret == 0) {
                data += fill;
                len  -= fill;
                left = 0U;
            }
        }

        if (ret == 0) {
             while (len >= 64U) {
                ret = mbedtls_internal_sha256_process(ctx, data);
                if (ret == 0) {
                    data += 64;
                    len  -= 64U;
                    break;
                }
            }

            if (ret == 0) {
                if (len > 0U) {
                    (void)memcpy_s((void *)&ctx->buffer[left], len, data, len);
                }
            }
        }
    }

    return ret;
}

/*
 * SHA-256 final digest
 */
int32_t mbedtls_sha256_finish_ret(mbedtls_sha256_context *ctx, uint8_t output[32])
{
    int32_t ret = 0;
    uint32_t used;
    uint32_t high, low;

    /*
     * Add padding: 0x80 then 0x00 until 8 bytes remain for the length
     */
    used = ctx->total[0] & 0x3FU;

    ctx->buffer[used] = 0x80U;

    used ++;

    if (used <= 56U) {
        /* Enough room for padding + length in current block */
        (void)memset((void *)&ctx->buffer[used], 0U, 56U - used);
    } else {
        /* We'll need an extra block */
        (void)memset((void *)&ctx->buffer[used], 0U, 64U - used);

        ret = mbedtls_internal_sha256_process(ctx, ctx->buffer);
        if (ret == 0) {
            (void)memset(ctx->buffer, 0U, 56U);
        }
    }

    /*
     * Add message length
     */
    if (ret == 0) {
        high = (ctx->total[0] >> 29)
            | (ctx->total[1] <<  3);
        low  = (ctx->total[0] <<  3);

        PUT_UINT32_BE(high, ctx->buffer, 56);
        PUT_UINT32_BE(low,  ctx->buffer, 60);

        ret = mbedtls_internal_sha256_process(ctx, ctx->buffer);
        if (ret == 0) {
            /*
             * Output final state
             */
            PUT_UINT32_BE(ctx->state[0], output,  0);
            PUT_UINT32_BE(ctx->state[1], output,  4);
            PUT_UINT32_BE(ctx->state[2], output,  8);
            PUT_UINT32_BE(ctx->state[3], output, 12);
            PUT_UINT32_BE(ctx->state[4], output, 16);
            PUT_UINT32_BE(ctx->state[5], output, 20);
            PUT_UINT32_BE(ctx->state[6], output, 24);

            if (ctx->is224 == 0) {
                PUT_UINT32_BE(ctx->state[7], output, 28);
            }
        }
    }

    return ret;
}

/*
 * output = SHA-256(input buffer)
 */
int32_t mbedtls_sha256_ret(const uint8_t *input, size_t ilen, uint8_t output[32], int32_t is224)
{
    int32_t ret = 0;
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);

    ret = mbedtls_sha256_starts_ret(&ctx, is224);
    if (ret == 0) {
        ret = mbedtls_sha256_update_ret(&ctx, input, ilen);
    }

    if (ret == 0) {
        ret = mbedtls_sha256_finish_ret(&ctx, output);
    }

    mbedtls_sha256_free(&ctx);

    return ret;
}
