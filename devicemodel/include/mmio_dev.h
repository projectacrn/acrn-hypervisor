/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _MMIO_DEV_H_
#define _MMIO_DEV_H_

int parse_pt_acpidev(char *arg);
int parse_pt_mmiodev(char *arg);

int init_mmio_devs(struct vmctx *ctx);
int deinit_mmio_devs(struct vmctx *ctx);

#endif /* _MMIO_DEV_H_ */
