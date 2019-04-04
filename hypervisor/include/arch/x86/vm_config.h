/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIG_H_
#define VM_CONFIG_H_

#include <types.h>
#include <pci.h>
#include <multiboot.h>
#include <acrn_common.h>
#include <vm_configurations.h>

#define PLUG_CPU(n)		(1U << (n))

/*
 * PRE_LAUNCHED_VM is launched by ACRN hypervisor, with LAPIC_PT;
 * SOS_VM is launched by ACRN hypervisor, without LAPIC_PT;
 * NORMAL_VM is launched by ACRN devicemodel, with/without LAPIC_PT depends on usecases.
 */
enum acrn_vm_type {
	UNDEFINED_VM = 0,
	PRE_LAUNCHED_VM,
	SOS_VM,
	NORMAL_VM	/* Post-launched VM */
};

struct acrn_vm_mem_config {
	uint64_t start_hpa;	/* the start HPA of VM memory configuration, for pre-launched VMs only */
	uint64_t size;		/* VM memory size configuration */
};

struct acrn_vm_os_config {
	char name[MAX_VM_OS_NAME_LEN];			/* OS name, useful for debug */
	char bootargs[MAX_BOOTARGS_SIZE];		/* boot args/cmdline */
} __aligned(8);

struct acrn_vm_pci_ptdev_config {
	union pci_bdf vbdf;				/* virtual BDF of PCI PT device */
	union pci_bdf pbdf;				/* physical BDF of PCI PT device */
} __aligned(8);

struct acrn_vm_config {
	enum acrn_vm_type type;				/* specify the type of VM */
	char name[MAX_VM_OS_NAME_LEN];			/* VM name identifier, useful for debug. */
	uint8_t GUID[16];				/* GUID of the VM */
	uint64_t pcpu_bitmap;				/* from pcpu bitmap, we could know VM core number */
	uint64_t guest_flags;				/* VM flags that we want to configure for guest
							 * Now we have two flags:
							 *	GUEST_FLAG_SECURE_WORLD_ENABLED
							 *	GUEST_FLAG_LAPIC_PASSTHROUGH
							 * We could add more guest flags in future;
							 */
	struct acrn_vm_mem_config memory;		/* memory configuration of VM */
	uint16_t pci_ptdev_num;				/* indicate how many PCI PT devices in VM */
	struct acrn_vm_pci_ptdev_config *pci_ptdevs;	/* point to PCI PT devices BDF list */
	struct acrn_vm_os_config os_config;		/* OS information the VM */
	uint16_t clos;					/* if guest_flags has GUEST_FLAG_CLOS_REQUIRED, then VM use this CLOS */

	bool			vm_vuart;
} __aligned(8);

struct acrn_vm_config *get_vm_config(uint16_t vm_id);
int32_t sanitize_vm_config(void);

extern struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM];

#endif /* VM_CONFIG_H_ */
