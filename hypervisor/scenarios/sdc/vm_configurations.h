/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <misc_cfg.h>

/* SOS_VM_NUM can only be 0U or 1U;
 * When SOS_VM_NUM is 0U, MAX_POST_VM_NUM must be 0U too;
 * MAX_POST_VM_NUM must be bigger than CONFIG_MAX_KATA_VM_NUM;
 */
#define PRE_VM_NUM			0U
#define SOS_VM_NUM			1U
#define MAX_POST_VM_NUM			2U	/* including 1 KATA VM */
#define CONFIG_MAX_KATA_VM_NUM		1U

/* Bits mask of guest flags that can be programmed by device model. Other bits are set by hypervisor only */
#define DM_OWNED_GUEST_FLAG_MASK	(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \
						GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)

#define SOS_VM_BOOTARGS			SOS_ROOTFS	\
					"rw rootwait "	\
					"console=tty0 " \
					SOS_CONSOLE	\
					"consoleblank=0 "	\
					"no_timer_check "	\
					"quiet loglevel=3 "	\
					"i915.nuclear_pageflip=1 " \
					"i915.avail_planes_per_pipe=0x01010F "	\
					"i915.domain_plane_owners=0x011111110000 " \
					"i915.enable_gvt=1 "	\
					SOS_IDLE \
					SOS_BOOTARGS_DIFF

#if CONFIG_MAX_KATA_VM_NUM > 0
  #define VM1_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(1U) | AFFINITY_CPU(2U))
  #define VM2_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(3U))
#else
  #define VM1_CONFIG_CPU_AFFINITY	(AFFINITY_CPU(1U) | AFFINITY_CPU(2U) | AFFINITY_CPU(3U))
#endif
#endif /* VM_CONFIGURATIONS_H */
