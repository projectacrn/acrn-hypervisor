/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <misc_cfg.h>
#include <pci_devices.h>

/* SOS_VM_NUM can only be 0U or 1U;
 * When SOS_VM_NUM is 0U, MAX_POST_VM_NUM must be 0U too;
 * MAX_POST_VM_NUM must be bigger than CONFIG_MAX_KATA_VM_NUM;
 */
#define PRE_VM_NUM			1U
#define SOS_VM_NUM			1U
#define MAX_POST_VM_NUM			1U
#define CONFIG_MAX_KATA_VM_NUM		0U

/* Bits mask of guest flags that can be programmed by device model. Other bits are set by hypervisor only */
#define DM_OWNED_GUEST_FLAG_MASK        (GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \
						GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)

#define VM0_CONFIG_CPU_AFFINITY         (AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#define VM0_CONFIG_MEM_START_HPA        0x100000000UL
#define VM0_CONFIG_MEM_SIZE             0x40000000UL
#define VM0_CONFIG_MEM_START_HPA2       0x0UL
#define VM0_CONFIG_MEM_SIZE_HPA2        0x0UL
#define VM0_CONFIG_PCI_DEV_NUM          4U

/* SOS_VM == VM1 */
#define SOS_VM_BOOTARGS			SOS_ROOTFS	\
					SOS_CONSOLE	\
					SOS_IDLE	\
					SOS_BOOTARGS_DIFF

#define SOS_VM_CONFIG_CPU_AFFINITY      (AFFINITY_CPU(0U) | AFFINITY_CPU(1U))

#define VM2_CONFIG_CPU_AFFINITY         (AFFINITY_CPU(1U))
#define VM2_CONFIG_PCI_DEV_NUM          1U

#endif /* VM_CONFIGURATIONS_H */
