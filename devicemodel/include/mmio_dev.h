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
void deinit_mmio_devs(struct vmctx *ctx);

int mmio_dev_alloc_gpa_resource32(uint32_t *addr, uint32_t size_in);
uint64_t get_mmio_dev_tpm2_base_gpa(void);

#define MMIO_DEV_BASE  0xF0000000U
#define MMIO_DEV_LIMIT 0xFE000000U
#endif /* _MMIO_DEV_H_ */
