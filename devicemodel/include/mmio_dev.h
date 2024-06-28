/*
 * Copyright (C) 2020-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _MMIO_DEV_H_
#define _MMIO_DEV_H_

#include "acrn_common.h"

struct mmio_dev {
	char name[16];
	struct acrn_mmiodev dev;
};

struct mmio_dev *get_mmiodev(char *name);
struct mmio_dev *alloc_mmiodev(void);
bool get_mmio_hpa_resource(char *name, uint64_t *res_start, uint64_t *res_size, uint8_t region_index);

int create_pt_mmiodev(char *arg);

int init_mmio_devs(struct vmctx *ctx);
void deinit_mmio_devs(struct vmctx *ctx);

int mmio_dev_alloc_gpa_resource32(uint32_t *addr, uint32_t size_in);

#define MMIO_DEV_BASE  0xF0000000U
#define MMIO_DEV_LIMIT 0xFE000000U
#endif /* _MMIO_DEV_H_ */
