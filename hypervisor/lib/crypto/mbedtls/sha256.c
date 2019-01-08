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

static const uint32_t k[] =
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

/**
 * @brief get unsinged int value for big endian.
 *
 * @param[in] b pointer to data which is NON-NULL
 */
static inline uint32_t get_uint32_be(const uint8_t *b, uint32_t i)
{
    uint32_t n;

    n = ((uint32_t) (*(b + i)) << 24)
         | ((uint32_t) (*(b + i + 1U)) << 16)
         | ((uint32_t) (*(b + i + 2U)) << 8)
         | ((uint32_t) (*(b + i + 3U)));

    return n;
}

/**
 * @brief put unsinged int value for big endian.
 * @param[inout] b pointer to data which is NON-NULL
 */
static inline void put_unint32_be(uint32_t n, uint8_t *b, uint32_t i)
{
    *(b + i) = (uint8_t) (n >> 24);
    *(b + i + 1U) = (uint8_t) (n >> 16);
    *(b + i + 2U) = (uint8_t) (n >> 8);
    *(b + i + 3U) = (uint8_t) n;
}

static inline uint32_t shr(uint32_t x, uint8_t n)
{
    return ((x & 0xFFFFFFFFU) >> n);
}

static inline uint32_t port(uint32_t x, uint8_t n)
{
    return (shr(x, n) | (x << (32U - n)));
}

static inline uint32_t s0(uint32_t x)
{
    return (port(x, 7U) ^ port(x, 18U) ^  shr(x, 3U));
}

static inline uint32_t s1(uint32_t x)
{
    return (port(x, 17U) ^ port(x, 19U) ^  shr(x, 10U));
}

static inline uint32_t s2(uint32_t x)
{
    return (port(x, 2U) ^ port(x, 13U) ^ port(x, 22U));
}

static inline uint32_t s3(uint32_t x)
{
    return (port(x, 6U) ^ port(x, 11U) ^ port(x, 25U));
}

static inline uint32_t f0(uint32_t x, uint32_t y, uint32_t z)
{
    return ((x & y) | (z & (x | y)));
}

static inline uint32_t f1(uint32_t x, uint32_t y, uint32_t z)
{
    return (z ^ (x & (y ^ z)));
}

static inline void r(uint32_t *w, uint32_t i)
{
    *(w + i) = s1(*(w  + i - 2U)) + (*(w + i - 7U)) + s0(*(w + i - 15U)) + (*(w + i - 16U));
}

/**
 * @brief Part of compress.
 *
 * @param[inout] d and h are NON-null pointer
 */
static inline void p( uint32_t a, uint32_t b, uint32_t c,
        uint32_t *d, uint32_t e, uint32_t f, uint32_t g, uint32_t *h, uint32_t x, uint32_t j)
{
    uint32_t temp1, temp2;

    temp1 = *h + s3(e) + f1(e, f, g) + j + x;
    temp2 = s2(a) + f0(a, b, c);
    *d += temp1; *h = temp1 + temp2;
}

void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    (void)memset(ctx, 0U, sizeof(mbedtls_sha256_context));
}

void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    if (ctx != NULL) {
        (void)mbedtls_platform_zeroize(ctx, sizeof(mbedtls_sha256_context));
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

    return 0;
}

int32_t mbedtls_internal_sha256_process(mbedtls_sha256_context *ctx, const uint8_t data[64])
{
    uint32_t w[64];
    uint32_t a[8];
    uint32_t i;

    for (i = 0U; i < 8U; i++) {
        a[i] = ctx->state[i];
    }

    for (i = 0U; i < 16U; i++) {
        w[i] = get_uint32_be(data, 4 * i);
    }

    for (i = 0U; i < 16U; i += 8U) {
        p(a[0], a[1], a[2], &a[3], a[4], a[5], a[6], &a[7], w[i + 0U], k[i + 0U]);
        p(a[7], a[0], a[1], &a[2], a[3], a[4], a[5], &a[6], w[i + 1U], k[i + 1U]);
        p(a[6], a[7], a[0], &a[1], a[2], a[3], a[4], &a[5], w[i + 2U], k[i + 2U]);
        p(a[5], a[6], a[7], &a[0], a[1], a[2], a[3], &a[4], w[i + 3U], k[i + 3U]);
        p(a[4], a[5], a[6], &a[7], a[0], a[1], a[2], &a[3], w[i + 4U], k[i + 4U]);
        p(a[3], a[4], a[5], &a[6], a[7], a[0], a[1], &a[2], w[i + 5U], k[i + 5U]);
        p(a[2], a[3], a[4], &a[5], a[6], a[7], a[0], &a[1], w[i + 6U], k[i + 6U]);
        p(a[1], a[2], a[3], &a[4], a[5], a[6], a[7], &a[0], w[i + 7U], k[i + 7U]);
    }

    for (i = 16U; i < 64U; i += 8U) {
        r(w, (i + 0U));
        p(a[0], a[1], a[2], &a[3], a[4], a[5], a[6], &a[7], w[i + 0U], k[i + 0U]);

        r(w, (i + 1U));
        p(a[7], a[0], a[1], &a[2], a[3], a[4], a[5], &a[6], w[i + 1U], k[i + 1U]);

        r(w, (i + 2U));
        p(a[6], a[7], a[0], &a[1], a[2], a[3], a[4], &a[5], w[i + 2U], k[i + 2U]);

        r(w, (i + 3U));
        p(a[5], a[6], a[7], &a[0], a[1], a[2], a[3], &a[4], w[i + 3U], k[i + 3U]);

        r(w, (i + 4U));
        p(a[4], a[5], a[6], &a[7], a[0], a[1], a[2], &a[3], w[i + 4U], k[i + 4U]);

        r(w, (i + 5U));
        p(a[3], a[4], a[5], &a[6], a[7], a[0], a[1], &a[2], w[i + 5U], k[i + 5U]);

        r(w, (i + 6U));
        p(a[2], a[3], a[4], &a[5], a[6], a[7], a[0], &a[1], w[i + 6U], k[i + 6U]);

        r(w, (i + 7U));
        p(a[1], a[2], a[3], &a[4], a[5], a[6], a[7], &a[0], w[i + 7U], k[i + 7U]);
    }

    for (i = 0U; i < 8U; i++) {
        ctx->state[i] += a[i];
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

    used++;

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

        put_unint32_be(high, ctx->buffer, 56);
        put_unint32_be(low,  ctx->buffer, 60);

        ret = mbedtls_internal_sha256_process(ctx, ctx->buffer);
        if (ret == 0) {
            /*
             * Output final state
             */
            put_unint32_be(ctx->state[0], output,  0);
            put_unint32_be(ctx->state[1], output,  4);
            put_unint32_be(ctx->state[2], output,  8);
            put_unint32_be(ctx->state[3], output, 12);
            put_unint32_be(ctx->state[4], output, 16);
            put_unint32_be(ctx->state[5], output, 20);
            put_unint32_be(ctx->state[6], output, 24);

            if (ctx->is224 == 0) {
                put_unint32_be(ctx->state[7], output, 28);
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
