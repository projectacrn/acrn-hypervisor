/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MMIO_DEV_H
#define MMIO_DEV_H

int32_t assign_mmio_dev(struct acrn_vm *vm, const struct acrn_mmiodev *mmiodev);
int32_t deassign_mmio_dev(struct acrn_vm *vm, const struct acrn_mmiodev *mmiodev);

#endif /* MMIO_DEV_H */
