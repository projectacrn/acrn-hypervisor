/*
 * Project Acrn
 * Acrn-dm: pm-vuart
 *
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef	__PM_VUART__
#define	__PM_VUART__

int parse_pm_by_vuart(const char *opts);
void pm_by_vuart_init(struct vmctx *ctx);
void pm_by_vuart_deinit(struct vmctx *ctx);

#endif
