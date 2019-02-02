/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PARTITION_CONFIG_H
#define PARTITION_CONFIG_H

#define PRE_LAUNCH_VM_NUM	2

#define	VM0_CONFIGURED

#define VM0_CONFIG_NAME				"PRE-LAUNCHED VM1 for DNV-CB2"
#define VM0_CONFIG_TYPE				PRE_LAUNCHED_VM
#define VM0_CONFIG_PCPU_BITMAP			(PLUG_CPU(0) | PLUG_CPU(2) | PLUG_CPU(4) | PLUG_CPU(6))
#define VM0_CONFIG_FLAGS			LAPIC_PASSTHROUGH | IO_COMPLETION_POLLING
#define VM0_CONFIG_MEM_START_HPA		0x100000000UL
#define VM0_CONFIG_MEM_SIZE			0x80000000UL

#define VM0_CONFIG_OS_NAME			"ClearLinux 26600"
#define VM0_CONFIG_OS_BOOTARGS			"root=/dev/sda rw rootwait noxsave maxcpus=4 nohpet console=hvc0 " \
						"console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M "\
						"consoleblank=0 tsc=reliable xapic_phys  apic_debug"

#define	VM1_CONFIGURED

#define VM1_CONFIG_NAME				"PRE-LAUNCHED VM2 for DNV-CB2"
#define VM1_CONFIG_TYPE				PRE_LAUNCHED_VM
#define VM1_CONFIG_PCPU_BITMAP			(PLUG_CPU(1) | PLUG_CPU(3) | PLUG_CPU(5) | PLUG_CPU(7))
#define VM1_CONFIG_FLAGS			LAPIC_PASSTHROUGH | IO_COMPLETION_POLLING
#define VM1_CONFIG_MEM_START_HPA		0x180000000UL
#define VM1_CONFIG_MEM_SIZE			0x80000000UL

#define VM1_CONFIG_OS_NAME			"ClearLinux 26600"
#define VM1_CONFIG_OS_BOOTARGS			"root=/dev/sda2 rw rootwait noxsave maxcpus=4 nohpet console=hvc0 "\
							"console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M "\
							"consoleblank=0 tsc=reliable xapic_phys apic_debug"

#define VM0_CONFIG_PCI_PTDEV_NUM		3U
#define VM1_CONFIG_PCI_PTDEV_NUM		3U

extern struct acrn_vm_pci_ptdev_config vm0_pci_ptdevs[VM0_CONFIG_PCI_PTDEV_NUM];
extern struct acrn_vm_pci_ptdev_config vm1_pci_ptdevs[VM1_CONFIG_PCI_PTDEV_NUM];

#endif /* PARTITION_CONFIG_H */
