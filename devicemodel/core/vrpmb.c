/*
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <openssl/rand.h>

#include "types.h"
#include "vrpmb.h"

#define DRNG_MAX_RETRIES 5U

struct key_material {
	uint8_t key[RPMB_KEY_LEN];
	bool initialized;
};

static struct key_material vrkey = {
	.key = {0},
	.initialized = false
};

int get_vrpmb_key(uint8_t *out, size_t size)
{
	int ret;
	int i;

	if (!out) {
		fprintf(stderr, "%s: Invalid output pointer\n", __func__);
		return 0;
	}

	if (size != RPMB_KEY_LEN) {
		fprintf(stderr, "%s: Invalid input key size\n", __func__);
		return 0;
	}

	if ( vrkey.initialized == false ) {
		for (i = 0; i < DRNG_MAX_RETRIES; i++) {
			ret = RAND_bytes(vrkey.key, RPMB_KEY_LEN);
			if (ret == 1) {
				vrkey.initialized = true;
				break;
			}
		}

		if (vrkey.initialized != true) {
			fprintf(stderr, "%s: unable to generate random key!\n", __func__);
			return 0;
		}
	}

	memcpy(out, vrkey.key, size);
	return 1;
}
