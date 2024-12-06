/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <acrn_hv_defs.h>

void npk_log_setup(__unused struct hv_npk_log_param *param) {}
void npk_log_write(__unused const char *buf, __unused size_t len) {}

bool npk_need_log(__unused uint32_t severity) { return false; }
void npk_log(__unused char *buffer) {}
