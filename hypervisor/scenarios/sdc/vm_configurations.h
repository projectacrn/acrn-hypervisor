/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * This is a template of vm_configurations.h for sharing mode;
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#define	VM0_CONFIGURED

#define VM0_CONFIG_NAME				"ACRN SOS VM"
#define VM0_CONFIG_TYPE				SOS_VM
#define VM0_CONFIG_PCPU_BITMAP			0UL	/* PCPU Bitmap is reserved in SOS_VM */
#define VM0_CONFIG_FLAGS			GUEST_FLAG_IO_COMPLETION_POLLING
#define VM0_CONFIG_CLOS				0U
#define VM0_CONFIG_MEM_START_HPA		0UL
#define VM0_CONFIG_MEM_SIZE			CONFIG_SOS_RAM_SIZE
#define VM0_CONFIG_OS_NAME			"ACRN Service OS"
#define VM0_CONFIG_OS_BOOTARGS			"configured in devicemodel/samples/apl-mrb/sos_bootargs_xxxx.txt"

#define VM0_CONFIG_PCI_PTDEV_NUM		0U	/* PTDEV is reserved in SOS_VM */
extern struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];

#endif /* VM_CONFIGURATIONS_H */
