/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <misc_cfg.h>
#include <pci_devices.h>
/* SERVICE_VM_NUM can only be 0 or 1; When SERVICE_VM_NUM is 1, MAX_POST_VM_NUM must be 0 too. */
#define PRE_VM_NUM                    1U
#define SERVICE_VM_NUM                1U
#define MAX_POST_VM_NUM               6U
#define CONFIG_MAX_VM_NUM             8U
/* Bitmask of guest flags that can be programmed by device model. Other bits are set by hypervisor only. */
#define DM_OWNED_GUEST_FLAG_MASK      (GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)
#define VM0_CONFIG_MEM_START_HPA      0x100000000UL
#define VM0_CONFIG_MEM_SIZE           0x20000000UL
#define VM0_CONFIG_MEM_START_HPA2     0x0UL
#define VM0_CONFIG_MEM_SIZE_HPA2      0x0UL
/* SERVICE_VM == VM1 */
#define SERVICE_VM_OS_BOOTARGS        SERVICE_VM_ROOTFS SERVICE_VM_OS_CONSOLE SERVICE_VM_IDLE SERVICE_VM_BOOTARGS_DIFF

#endif /* VM_CONFIGURATIONS_H */
