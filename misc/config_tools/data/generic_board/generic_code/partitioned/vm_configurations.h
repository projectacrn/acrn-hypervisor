/*
 * Copyright (C) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <misc_cfg.h>
#include <pci_devices.h>
/* SERVICE_VM_NUM can only be 0 or 1; When SERVICE_VM_NUM is 1, MAX_POST_VM_NUM must be 0 too. */
#define PRE_VM_NUM 2U
#define SERVICE_VM_NUM 0U
#define MAX_POST_VM_NUM 14U
#define MAX_TRUSTY_VM_NUM 0U
#define CONFIG_MAX_VM_NUM 16U
#define MAX_VUART_NUM_PER_VM 2U
#define MAX_IR_ENTRIES 256U

#endif /* VM_CONFIGURATIONS_H */
