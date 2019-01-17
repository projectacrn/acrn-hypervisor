/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <sos_vm.h>

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE) = {
	{
		.type = SOS_VM,
		.name = SOS_VM_CONFIG_NAME,
		.pcpu_bitmap = SOS_VM_CONFIG_PCPU_BITMAP,
		.guest_flags = SOS_VM_CONFIG_GUEST_FLAGS,
		.memory = {
			.start_hpa = 0x0UL,
			.size = SOS_VM_CONFIG_MEM_SIZE,
		},
		.os_config = {
			.name = SOS_VM_CONFIG_OS_NAME,
		},
	},
};
