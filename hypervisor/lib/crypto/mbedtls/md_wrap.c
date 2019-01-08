/**
 * \file md_wrap.c
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
#include "md_internal.h"
#include "sha256.h"

/*
 * Wrappers for generic message digests
 */

static int32_t sha256_update_wrap( void *ctx, const uint8_t *input,
                                size_t ilen )
{
    return( mbedtls_sha256_update_ret( (mbedtls_sha256_context *) ctx,
                                       input, ilen ) );
}

static int32_t sha256_finish_wrap( void *ctx, uint8_t *output )
{
    return( mbedtls_sha256_finish_ret( (mbedtls_sha256_context *) ctx,
                                       output ) );
}

static void sha256_clone_wrap( void *dst, const void *src )
{
    mbedtls_sha256_clone( (mbedtls_sha256_context *) dst,
                    (const mbedtls_sha256_context *) src );
}

static int32_t sha256_process_wrap( void *ctx, const uint8_t *data )
{
    return( mbedtls_internal_sha256_process( (mbedtls_sha256_context *) ctx,
                                             data ) );
}

static int32_t sha256_starts_wrap( void *ctx )
{
    return( mbedtls_sha256_starts_ret( (mbedtls_sha256_context *) ctx, 0 ) );
}

static int32_t sha256_wrap( const uint8_t *input, size_t ilen,
                        uint8_t *output )
{
    return( mbedtls_sha256_ret( input, ilen, output, 0 ) );
}

const mbedtls_md_info_t mbedtls_sha256_info = {
    MBEDTLS_MD_SHA256,
    "SHA256",
    32,
    64U,
    sha256_starts_wrap,
    sha256_update_wrap,
    sha256_finish_wrap,
    sha256_wrap,
    sha256_clone_wrap,
    sha256_process_wrap,
};
