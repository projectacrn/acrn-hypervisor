/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_CONFIG_H_
#define VM_CONFIG_H_

#include <types.h>
#include <pci.h>
#include <board_info.h>
#include <boot.h>
#include <acrn_common.h>
#include <vm_configurations.h>
#include <acrn_hv_defs.h>
#include <schedule.h>
#include <asm/vm_config.h>
#include <vuart.h>

#define AFFINITY_CPU(n)		(1UL << (n))
#define MAX_VCPUS_PER_VM	MAX_PCPU_NUM
#define MAX_VM_OS_NAME_LEN	32U
#define MAX_MOD_TAG_LEN		32U

#if defined(CONFIG_SCHED_NOOP)
#define SERVICE_VM_IDLE		""
#elif defined(CONFIG_SCHED_PRIO)
#define SERVICE_VM_IDLE		""
#else
#define SERVICE_VM_IDLE		"idle=halt "
#endif

#define PCI_DEV_TYPE_NONE		0U
#define PCI_DEV_TYPE_PTDEV		(1U << 0U)
#define PCI_DEV_TYPE_HVEMUL		(1U << 1U)
#define PCI_DEV_TYPE_SERVICE_VM_EMUL	(1U << 2U)
#define PCI_DEV_TYPE_DUMMY_MF_EMUL	(1U << 3U)

#define MAX_MMIO_DEV_NUM	2U

#define CONFIG_SERVICE_VM	.load_order = SERVICE_VM,	\
				.severity = SEVERITY_SERVICE_VM

#define CONFIG_SAFETY_VM	.load_order = PRE_LAUNCHED_VM,	\
				.severity = SEVERITY_SAFETY_VM

#define CONFIG_PRE_STD_VM	.load_order = PRE_LAUNCHED_VM,	\
				.severity = SEVERITY_STANDARD_VM

#define CONFIG_PRE_RT_VM	.load_order = PRE_LAUNCHED_VM,	\
				.severity = SEVERITY_RTVM

#define CONFIG_POST_STD_VM	.load_order = POST_LAUNCHED_VM,	\
				.severity = SEVERITY_STANDARD_VM

#define CONFIG_POST_RT_VM	.load_order = POST_LAUNCHED_VM,	\
				.severity = SEVERITY_RTVM

/* ACRN guest severity */
enum acrn_vm_severity {
	SEVERITY_SAFETY_VM = 0x40U,
	SEVERITY_RTVM = RTVM_SEVERITY_LEVEL,
	SEVERITY_SERVICE_VM = 0x20U,
	SEVERITY_STANDARD_VM = 0x10U,
};

struct vm_hpa_regions {
	uint64_t start_hpa;
	uint64_t size_hpa;
};

struct acrn_vm_mem_config {
	uint64_t size;		/* VM memory size configuration */
	uint64_t region_num;
	struct vm_hpa_regions  *host_regions;
};

enum os_kernel_type {
	KERNEL_BZIMAGE = 1,
	KERNEL_RAWIMAGE,
	KERNEL_ELF,
	KERNEL_UNKNOWN,
};

struct acrn_vm_os_config {
	char name[MAX_VM_OS_NAME_LEN];			/* OS name, useful for debug */
	enum os_kernel_type kernel_type;		/* used for kernel specifc loading method */
	char kernel_mod_tag[MAX_MOD_TAG_LEN];		/* multiboot module tag for kernel */
	char ramdisk_mod_tag[MAX_MOD_TAG_LEN];		/* multiboot module tag for ramdisk */
	char bootargs[MAX_BOOTARGS_SIZE];		/* boot args/cmdline */
	uint64_t kernel_load_addr;
	uint64_t kernel_entry_addr;
	uint64_t kernel_ramdisk_addr;
} __aligned(8);

struct acrn_vm_acpi_config {
	char acpi_mod_tag[MAX_MOD_TAG_LEN];		/* multiboot module tag for ACPI */
} __aligned(8);

/* the vbdf is assgined by device model */
#define UNASSIGNED_VBDF        0xFFFFU

struct acrn_vm_pci_dev_config {
	uint32_t emu_type;				/* the type how the device is emulated. */
	union pci_bdf vbdf;				/* virtual BDF of PCI device */
	union pci_bdf pbdf;				/* physical BDF of PCI device */
	char shm_region_name[32];			/* TODO: combine pbdf and shm_region_name into a union member */
	/* TODO: All device specific attributions need move to other place */
	struct target_vuart t_vuart;
	uint16_t vuart_idx;
	uint16_t vrp_sec_bus;			/* use virtual root port's secondary bus as unique identification */
	uint8_t vrp_max_payload;		/* vrp's dev cap's max payload */
	uint64_t vbar_base[PCI_BAR_COUNT];		/* vbar base address of PCI device, which is power-on default value */
	struct pci_pdev *pdev;				/* the physical PCI device if it's a PT device */
	const struct pci_vdev_ops *vdev_ops;		/* operations for PCI CFG read/write */
} __aligned(8);

struct pt_intx_config {
	uint32_t phys_gsi;	/* physical IOAPIC gsi to be forwarded to the VM */
	uint32_t virt_gsi;	/* virtual IOAPIC gsi triggered on the vIOAPIC */
} __aligned(8);

struct acrn_vm_config {
	enum acrn_vm_load_order load_order;		/* specify the load order of VM */
	char name[MAX_VM_NAME_LEN];				/* VM name identifier */
	uint8_t reserved[2];
	uint8_t severity;				/* severity of the VM */
	uint64_t cpu_affinity;				/* The set bits represent the pCPUs the vCPUs of
							 * the VM may run on.
							 */
	uint64_t guest_flags;				/* VM flags that we want to configure for guest
							 * Now we have two flags:
							 *	GUEST_FLAG_SECURE_WORLD_ENABLED
							 *	GUEST_FLAG_LAPIC_PASSTHROUGH
							 * We could add more guest flags in future;
							 */
	struct sched_params sched_params;		/* Scheduler params for vCPUs of this VM */
	uint16_t companion_vm_id;			/* The companion VM id for this VM */
	struct acrn_vm_mem_config memory;		/* memory configuration of VM */
	uint16_t pci_dev_num;				/* indicate how many PCI devices in VM */
	struct acrn_vm_pci_dev_config *pci_devs;	/* point to PCI devices BDF list */
	struct acrn_vm_os_config os_config;		/* OS information the VM */
	struct acrn_vm_acpi_config acpi_config;		/* ACPI config for the VM */

	struct vuart_config vuart[MAX_VUART_NUM_PER_VM];/* vuart configuration for VM */

	struct acrn_mmiodev mmiodevs[MAX_MMIO_DEV_NUM];

	uint16_t pt_intx_num; /* number of pt_intx_config entries pointed by pt_intx */
	struct pt_intx_config *pt_intx; /* stores the base address of struct pt_intx_config array */

	struct arch_vm_config arch;
} __aligned(8);

struct acrn_vm_config *get_vm_config(uint16_t vm_id);
uint8_t get_vm_severity(uint16_t vm_id);
bool vm_has_matched_name(uint16_t vmid, const char *name);

extern struct acrn_vm_config vm_configs[CONFIG_MAX_VM_NUM];
extern struct acrn_vm_config *const service_vm_config;

#endif /* VM_CONFIG_H_ */
