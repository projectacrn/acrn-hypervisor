/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#define CONFIG_MAX_VM_NUM	3U

/* Bits mask of guest flags that can be programmed by device model. Other bits are set by hypervisor only */
#define DM_OWNED_GUEST_FLAG_MASK	(GUEST_FLAG_SECURE_WORLD_ENABLED | GUEST_FLAG_LAPIC_PASSTHROUGH | \
						GUEST_FLAG_RT | GUEST_FLAG_IO_COMPLETION_POLLING)


#endif /* VM_CONFIGURATIONS_H */
