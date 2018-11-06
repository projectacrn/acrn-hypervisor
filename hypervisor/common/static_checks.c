/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <acrn_common.h>

#define CAT__(A,B) A ## B
#define CAT_(A,B) CAT__(A,B)
#define CTASSERT(expr) \
typedef int CAT_(CTA_DummyType,__LINE__)[(expr) ? 1 : -1]

CTASSERT(sizeof(struct vhm_request) == (4096U/VHM_REQUEST_MAX));
