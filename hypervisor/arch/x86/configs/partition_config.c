/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm.h>
#include <partition_config.h>

#define INIT_VM_CONFIG(idx)	\
	{		\
		.type = VM##idx##_CONFIG_TYPE,	\
		.name = VM##idx##_CONFIG_NAME,	\
		.pcpu_bitmap = VM##idx##_CONFIG_PCPU_BITMAP,	\
		.guest_flags = VM##idx##_CONFIG_FLAGS,	\
		.memory = {	\
			.start_hpa = VM##idx##_CONFIG_MEM_START_HPA,	\
			.size = VM##idx##_CONFIG_MEM_SIZE,	\
		},	\
		.os_config = {	\
			.name = VM##idx##_CONFIG_OS_NAME,	\
			.bootargs = VM##idx##_CONFIG_OS_BOOTARGS,	\
		},	\
		.vm_vuart = true,	\
		.pci_ptdev_num = VM##idx##_CONFIG_PCI_PTDEV_NUM,	\
		.pci_ptdevs = vm##idx##_pci_ptdevs,	\
	},

struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM] __aligned(PAGE_SIZE) = {
#ifdef VM0_CONFIGURED
	INIT_VM_CONFIG(0)
#endif

#ifdef VM1_CONFIGURED
	INIT_VM_CONFIG(1)
#endif

#ifdef VM2_CONFIGURED
	INIT_VM_CONFIG(2)
#endif

#ifdef VM3_CONFIGURED
	INIT_VM_CONFIG(3)
#endif
};
