/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FIRMWARE_SBL_H

#define FIRMWARE_SBL_H

#include <firmware.h>

struct firmware_operations* sbl_get_firmware_operations(void);
int32_t sbl_init_vm_boot_info(struct acrn_vm *vm);

#endif /* end of include guard: FIRMWARE_SBL_H */
