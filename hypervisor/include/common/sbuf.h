/*
 * SHARED BUFFER
 *
 * Copyright (C) 2017-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Li Fei <fei1.li@intel.com>
 *
 */

#ifndef SHARED_BUFFER_H
#define SHARED_BUFFER_H
#include <acrn_common.h>
#include <asm/guest/vm.h>
/**
 *@pre sbuf != NULL
 *@pre data != NULL
 */
uint32_t sbuf_put(struct shared_buf *sbuf, uint8_t *data);
int32_t sbuf_share_setup(uint16_t cpu_id, uint32_t sbuf_id, uint64_t *hva);
void sbuf_reset(void);
uint32_t sbuf_next_ptr(uint32_t pos, uint32_t span, uint32_t scope);

#endif /* SHARED_BUFFER_H */
