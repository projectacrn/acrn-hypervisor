/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_H_
#define BOOT_H_

#include <multiboot.h>

#define MAX_BOOTARGS_SIZE		2048U

/* boot_regs store the multiboot info magic and address */
extern uint32_t boot_regs[2];

#endif /* BOOT_H_ */
