/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PARTITION_CONFIG_H
#define PARTITION_CONFIG_H

#define	VM0_CONFIGURED

#define VM0_CONFIG_NAME				"PRE-LAUNCHED VM1 for APL-MRB"
#define VM0_CONFIG_TYPE				PRE_LAUNCHED_VM
#define VM0_CONFIG_PCPU_BITMAP			(PLUG_CPU(0) | PLUG_CPU(2))
#define VM0_CONFIG_FLAGS			GUEST_FLAG_IO_COMPLETION_POLLING
#define VM0_CONFIG_MEM_START_HPA		0x100000000UL
#define VM0_CONFIG_MEM_SIZE			0x20000000UL

#define VM0_CONFIG_OS_NAME			"ClearLinux 26600"
#define VM0_CONFIG_OS_BOOTARGS			"root=/dev/sda3 rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS2 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys"

#define	VM1_CONFIGURED

#define VM1_CONFIG_NAME				"PRE-LAUNCHED VM2 for APL-MRB"
#define VM1_CONFIG_TYPE				PRE_LAUNCHED_VM
#define VM1_CONFIG_PCPU_BITMAP			(PLUG_CPU(1) | PLUG_CPU(3))
#define VM1_CONFIG_FLAGS			GUEST_FLAG_IO_COMPLETION_POLLING
#define VM1_CONFIG_MEM_START_HPA		0x120000000UL
#define VM1_CONFIG_MEM_SIZE			0x20000000UL

#define VM1_CONFIG_OS_NAME			"ClearLinux 26600"
#define VM1_CONFIG_OS_BOOTARGS			"root=/dev/sda3 rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
						console=ttyS2 no_timer_check ignore_loglevel log_buf_len=16M \
						consoleblank=0 tsc=reliable xapic_phys"

#define VM0_CONFIG_PCI_PTDEV_NUM		3U
#define VM1_CONFIG_PCI_PTDEV_NUM		3U

extern struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];
extern struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[VM1_CONFIG_PCI_PTDEV_NUM];

#endif /* PARTITION_CONFIG_H */
