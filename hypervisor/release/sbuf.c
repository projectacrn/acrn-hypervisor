/*
 * Copyright (C) 2019-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>

int32_t sbuf_share_setup(__unused uint16_t pcpu_id,
		__unused uint32_t sbuf_id, __unused uint64_t *hva)
{
	return -EPERM;
}
void sbuf_reset(void) {}
