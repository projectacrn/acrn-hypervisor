/**
 * \file md_internal.h
 *
 * \brief Message digest wrappers.
 *
 * \warning This in an internal header. Do not include directly.
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 */
/*
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
#ifndef MBEDTLS_MD_WRAP_H
#define MBEDTLS_MD_WRAP_H

#include "md.h"

/**
 * Message digest information.
 * Allows message digest functions to be called in a generic way.
 */
struct mbedtls_md_info
{
    /** Digest identifier */
    mbedtls_md_type_t type;

    /** Name of the message digest */
    const char * name;

    /** Output length of the digest function in bytes */
    int32_t size;

    /** Block length of the digest function in bytes */
    size_t block_size;

    /** Digest initialisation function */
    int32_t (*starts_func)( void *ctx );

    /** Digest update function */
    int32_t (*update_func)( void *ctx, const uint8_t *input, size_t ilen );

    /** Digest finalisation function */
    int32_t (*finish_func)( void *ctx, uint8_t *output );

    /** Generic digest function */
    int32_t (*digest_func)( const uint8_t *input, size_t ilen,
                        uint8_t *output );

    /** Clone state from a context */
    void (*clone_func)( void *dst, const void *src );

    /** Internal use only */
    int32_t (*process_func)( void *ctx, const uint8_t *input );
};

extern const mbedtls_md_info_t mbedtls_sha256_info;

#endif /* MBEDTLS_MD_WRAP_H */
