/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <misc_cfg.h>

#define CONFIG_MAX_VM_NUM		(2U + CONFIG_MAX_KATA_VM_NUM)

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
					SOS_BOOTARGS_DIFF

#endif /* VM_CONFIGURATIONS_H */
