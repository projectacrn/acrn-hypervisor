/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/**
 * @file hsm_ioctl_defs.h
 *
 * @brief Hypervisor Module definition for ioctl to user space
 */

#ifndef	_VHM_IOCTL_DEFS_H_
#define	_VHM_IOCTL_DEFS_H_

#include <linux/types.h>

/* Commmon structures for ACRN/HSM/DM */
#include "acrn_common.h"

/* The ioctl type, documented in ioctl-number.rst */
#define ACRN_IOCTL_TYPE			0xA2

/*
 * Commmon IOCTL ID defination for HSM/DM
 */
#define _IC_ID(x, y) (((x)<<24)|(y))
#define IC_ID 0x43UL

/*
 * Common IOCTL IDs definition for ACRN userspace
 */
#define ACRN_IOCTL_GET_PLATFORM_INFO	\
	_IOR(ACRN_IOCTL_TYPE, 0x03, struct acrn_platform_info)

#define IC_ID_VM_BASE                  0x10UL
#define ACRN_IOCTL_CREATE_VM		\
	_IOWR(ACRN_IOCTL_TYPE, 0x10, struct acrn_vm_creation)
#define ACRN_IOCTL_DESTROY_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x11)
#define ACRN_IOCTL_START_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x12)
#define ACRN_IOCTL_PAUSE_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x13)
#define ACRN_IOCTL_RESET_VM		\
	_IO(ACRN_IOCTL_TYPE, 0x15)

#define IC_SET_VCPU_REGS               _IC_ID(IC_ID, IC_ID_VM_BASE + 0x06)

/* IRQ and Interrupts */
#define IC_ID_IRQ_BASE                 0x20UL
#define IC_INJECT_MSI                  _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x03)
#define IC_VM_INTR_MONITOR             _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x04)
#define IC_SET_IRQLINE                 _IC_ID(IC_ID, IC_ID_IRQ_BASE + 0x05)

/* DM ioreq management */
#define IC_ID_IOREQ_BASE                0x30UL
#define IC_SET_IOREQ_BUFFER             _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x00)
#define IC_NOTIFY_REQUEST_FINISH        _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x01)
#define IC_CREATE_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x02)
#define IC_ATTACH_IOREQ_CLIENT          _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x03)
#define IC_DESTROY_IOREQ_CLIENT         _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x04)
#define IC_CLEAR_VM_IOREQ               _IC_ID(IC_ID, IC_ID_IOREQ_BASE + 0x05)

/* Guest memory management */
#define IC_ID_MEM_BASE                  0x40UL
/* IC_ALLOC_MEMSEG not used */
#define IC_ALLOC_MEMSEG                 _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x00)
#define IC_SET_MEMSEG                   _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x01)
#define IC_UNSET_MEMSEG                 _IC_ID(IC_ID, IC_ID_MEM_BASE + 0x02)

/* PCI assignment*/
#define IC_ID_PCI_BASE                  0x50UL
#define IC_ASSIGN_PTDEV                _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x00)
#define IC_DEASSIGN_PTDEV              _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x01)
#define IC_VM_PCI_MSIX_REMAP           _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x02)
#define IC_SET_PTDEV_INTR_INFO         _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x03)
#define IC_RESET_PTDEV_INTR_INFO       _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x04)
#define IC_ASSIGN_PCIDEV               _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x05)
#define IC_DEASSIGN_PCIDEV             _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x06)
#define IC_ASSIGN_MMIODEV              _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x07)
#define IC_DEASSIGN_MMIODEV            _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x08)
#define IC_ADD_HV_VDEV                 _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x09)
#define IC_REMOVE_HV_VDEV              _IC_ID(IC_ID, IC_ID_PCI_BASE + 0x0A)

/* Power management */
#define IC_ID_PM_BASE                   0x60UL
#define IC_PM_GET_CPU_STATE            _IC_ID(IC_ID, IC_ID_PM_BASE + 0x00)

#define VM_MEMMAP_SYSMEM       0
#define VM_MMIO         1

/* VHM eventfd */
#define IC_ID_EVENT_BASE		0x70UL
#define IC_EVENT_IOEVENTFD		_IC_ID(IC_ID, IC_ID_EVENT_BASE + 0x00)
#define IC_EVENT_IRQFD			_IC_ID(IC_ID, IC_ID_EVENT_BASE + 0x01)

/**
 * @brief EPT memory mapping info for guest
 */
struct vm_memmap {
	/** memory mapping type */
	uint32_t type;
	/** using vma_base to get sos_vm_gpa,
	 * only for type == VM_MEMMAP_SYSMEM
	 */
	uint32_t using_vma;
	/** user OS guest physical start address of memory mapping */
	uint64_t gpa;
	union {
		/** host physical start address of memory,
		 * only for type == VM_MMIO
		 */
		uint64_t hpa;
		/** service OS user virtual start address of
		 * memory, only for type == VM_MEMMAP_SYSMEM &&
		 * using_vma == true
		 */
		uint64_t vma_base;
	};
	/** the length of memory range mapped */
	uint64_t len;	/* mmap length */
	/** memory mapping attribute */
	uint32_t prot;	/* RWX */
};

/**
 * @brief Info to assign or deassign PCI for a VM
 *
 */
struct acrn_assign_pcidev {
#define QUIRK_PTDEV	(1 << 0)	/* We will only handle general part in HV, others in DM */
	/** the type of the the pass-through PCI device */
	uint32_t type;

	/** virtual BDF# of the pass-through PCI device */
	uint16_t virt_bdf;

	/** physical BDF# of the pass-through PCI device */
	uint16_t phys_bdf;

	/** the PCI Interrupt Line, initialized by ACRN-DM, which is RO and
	 *  ideally not used for pass-through MSI/MSI-x devices.
	 */
	uint8_t intr_line;

	/** the PCI Interrupt Pin, initialized by ACRN-DM, which is RO and
	 *  ideally not used for pass-through MSI/MSI-x devices.
	 */
	uint8_t intr_pin;

	/** the base address of the PCI BAR, initialized by ACRN-DM. */
	uint32_t bar[6];

	/** reserved for extension */
	uint32_t rsvd2[6];

} __attribute__((aligned(8)));

/**
 * @brief Info to assign or deassign a MMIO device for a VM
 */
struct acrn_mmiodev {
	/** the gpa of the MMIO region for the MMIO device */
	uint64_t base_gpa;

	/** the hpa of the MMIO region for the MMIO device */
	uint64_t base_hpa;

	/** the size of the MMIO region for the MMIO device */
	uint64_t size;

	/** reserved for extension */
	uint64_t reserved[13];

} __attribute__((aligned(8)));

/**
 * @brief Info to create or destroy a virtual PCI or legacy device for a VM
 *
 * the parameter for HC_CREATE_VDEV or HC_DESTROY_VDEV hypercall
 */
struct acrn_emul_dev {
	/*
	 * the identifier of the device, the low 32 bits represent the vendor
	 * id and device id of PCI device and the high 32 bits represent the
	 * device number of the legacy device
	 */
	union dev_id_info {
		uint64_t value;
		struct fields_info {
			uint16_t vendor_id;
			uint16_t device_id;
			uint32_t legacy_device_number;
		} fields;
	} dev_id;

	/*
	 * the slot of the device, if the device is a PCI device, the slot
	 * represents BDF, otherwise it represents legacy device slot number
	 */
	uint32_t slot;

	/** reserved for extension */
	uint32_t reserved0;

	/** the IO resource address of the device, initialized by ACRN-DM. */
	uint32_t io_addr[6];

	/** the IO resource size of the device, initialized by ACRN-DM. */
	uint32_t io_size[6];

	/** the options for the virtual device, initialized by ACRN-DM. */
	uint8_t args[128];

	/** reserved for extension */
	uint64_t reserved1[8];

} __attribute__((aligned(8)));

/**
 * @brief pass thru device irq data structure
 */
struct ic_ptdev_irq {
#define IRQ_INTX 0
#define IRQ_MSI 1
#define IRQ_MSIX 2
	/** irq type */
	uint32_t type;
	/** virtual bdf description of pass thru device */
	uint16_t virt_bdf;	/* IN: Device virtual BDF# */
	/** physical bdf description of pass thru device */
	uint16_t phys_bdf;	/* IN: Device physical BDF# */

	/** info of IOAPIC/PIC interrupt */
	struct {
		/** virtual IOAPIC pin */
		uint32_t virt_pin;
		/** physical IOAPIC pin */
		uint32_t phys_pin;
		/** PIC pin */
		uint32_t is_pic_pin;
	} intx;
};

/**
 * @brief data strcture to notify hypervisor ioreq is handled
 */
struct ioreq_notify {
	/** client id to identify ioreq client */
	int32_t client_id;
	/** identify the ioreq submitter */
	uint32_t vcpu;
};

#define ACRN_PLATFORM_LAPIC_IDS_MAX	64
/**
 * struct acrn_platform_info - Information of a platform from hypervisor
 * @hw.cpu_num:			Physical CPU number of the platform
 * @hw.version:			Version of this structure
 * @hw.l2_cat_shift:		Order of the number of threads sharing L2 cache
 * @hw.l3_cat_shift:		Order of the number of threads sharing L3 cache
 * @hw.lapic_ids:		IDs of LAPICs of all threads
 * @hw.reserved:		Reserved for alignment and should be 0
 * @sw.max_vcpus_per_vm:	Maximum number of vCPU of a VM
 * @sw.max_vms:			Maximum number of VM
 * @sw.vm_config_size:		Size of configuration of a VM
 * @sw.vm_configss_addr:	Memory address which user space provided to
 *				store the VM configurations
 * @sw.max_kata_containers:	Maximum number of VM for Kata containers
 * @sw.reserved:		Reserved for alignment and should be 0
 *
 * If vm_configs_addr is provided, the driver uses a bounce buffer (kmalloced
 * for continuous memory region) to fetch VM configurations data from the
 * hypervisor.
 */
struct acrn_platform_info {
	struct {
		__u16	cpu_num;
		__u16	version;
		__u32	l2_cat_shift;
		__u32	l3_cat_shift;
		__u8	lapic_ids[ACRN_PLATFORM_LAPIC_IDS_MAX];
		__u8	reserved[52];
	} hw;

	struct {
		__u16	max_vcpus_per_vm;
		__u16	max_vms;
		__u32	vm_config_size;
		void	*vm_configs_addr;
		__u64	max_kata_containers;
		__u8	reserved[104];
	} sw;
};

struct acrn_ioeventfd {
#define ACRN_IOEVENTFD_FLAG_PIO		0x01
#define ACRN_IOEVENTFD_FLAG_DATAMATCH	0x02
#define ACRN_IOEVENTFD_FLAG_DEASSIGN	0x04
       /** file descriptor of the eventfd of this ioeventfd */
       int32_t fd;
       /** flag for ioeventfd ioctl */
       uint32_t flags;
       /** base address to be monitored */
       uint64_t addr;
       /** address length */
       uint32_t len;
       uint32_t reserved;
       /** data to be matched */
       uint64_t data;
};

struct acrn_irqfd {
#define ACRN_IRQFD_FLAG_DEASSIGN	0x01
       /** file descriptor of the eventfd of this irqfd */
       int32_t fd;
       /** flag for irqfd ioctl */
       uint32_t flags;
       /** MSI interrupt to be injected */
       struct acrn_msi_entry msi;
};
#endif /* VHM_IOCTL_DEFS_H */
