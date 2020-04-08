/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIGURATIONS_H
#define VM_CONFIGURATIONS_H

#include <pci_devices.h>
#include <misc_cfg.h>

/* Bits mask of guest flags that can be programmed by device model. Other bits are set by hypervisor only */
#define DM_OWNED_GUEST_FLAG_MASK	0UL

/* SOS_VM_NUM can only be 0U or 1U;
 * When SOS_VM_NUM is 0U, MAX_POST_VM_NUM must be 0U too;
 * MAX_POST_VM_NUM must be bigger than CONFIG_MAX_KATA_VM_NUM;
 */
#define PRE_VM_NUM			2U
#define SOS_VM_NUM			0U
#define MAX_POST_VM_NUM			0U
#define CONFIG_MAX_KATA_VM_NUM		0U

/* The VM CONFIGs like:
 *	VMX_CONFIG_CPU_AFFINITY
 *	VMX_CONFIG_MEM_START_HPA
 *	VMX_CONFIG_MEM_SIZE
 *	VMX_CONFIG_OS_BOOTARG_ROOT
 *	VMX_CONFIG_OS_BOOTARG_MAX_CPUS
 *	VMX_CONFIG_OS_BOOTARG_CONSOLE
 * might be different on your board, please modify them per your needs.
 */

#define VM0_CONFIG_CPU_AFFINITY			(AFFINITY_CPU(0U) | AFFINITY_CPU(2U))
#define VM0_CONFIG_MEM_START_HPA		0x100000000UL
#define VM0_CONFIG_MEM_SIZE			0x20000000UL
#define VM0_CONFIG_OS_BOOTARG_ROOT		ROOTFS_0
#define VM0_CONFIG_OS_BOOTARG_MAXCPUS		"maxcpus=2 "
#define VM0_CONFIG_OS_BOOTARG_CONSOLE		"console=ttyS0 "

#define VM1_CONFIG_CPU_AFFINITY			(AFFINITY_CPU(1U) | AFFINITY_CPU(3U))
#define VM1_CONFIG_MEM_START_HPA		0x120000000UL
#define VM1_CONFIG_MEM_SIZE			0x20000000UL
#define VM1_CONFIG_OS_BOOTARG_ROOT		ROOTFS_0
#define VM1_CONFIG_OS_BOOTARG_MAXCPUS		"maxcpus=2 "
#define VM1_CONFIG_OS_BOOTARG_CONSOLE		"console=ttyS0 "

/* VM pass-through devices assign policy:
 * VM0: one Mass Storage controller, one Network controller;
 * VM1: one Mass Storage controller, one Network controller(if a secondary Network controller class device exist);
 */
#define VM0_STORAGE_CONTROLLER			SATA_CONTROLLER_0
#define VM0_NETWORK_CONTROLLER			ETHERNET_CONTROLLER_0
#define VM0_CONFIG_PCI_DEV_NUM			3U

#define VM1_STORAGE_CONTROLLER			USB_CONTROLLER_0
#if defined(ETHERNET_CONTROLLER_1)
/* if a secondary Ethernet controller subclass exist, assign to VM1 */
#define VM1_NETWORK_CONTROLLER			ETHERNET_CONTROLLER_1
#elif defined(NETWORK_CONTROLLER_0)
/* if a Network controller subclass exist(usually it is a wireless network card), assign to VM1 */
#define VM1_NETWORK_CONTROLLER			NETWORK_CONTROLLER_0
#endif

#if defined(VM1_NETWORK_CONTROLLER)
#define VM1_CONFIG_PCI_DEV_NUM			3U
#else
/* no network controller could be assigned to VM1 */
#define VM1_CONFIG_PCI_DEV_NUM			2U
#endif

#endif /* VM_CONFIGURATIONS_H */
