/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2017-2022 Intel Corporation.
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
 * Copyright (C) 2017-2022 Intel Corporation.
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
#define ACRN_IOCTL_SET_VCPU_REGS	\
	_IOW(ACRN_IOCTL_TYPE, 0x16, struct acrn_vcpu_regs)

/* IRQ and Interrupts */
#define ACRN_IOCTL_INJECT_MSI		\
	_IOW(ACRN_IOCTL_TYPE, 0x23, struct acrn_msi_entry)
#define ACRN_IOCTL_VM_INTR_MONITOR	\
	_IOW(ACRN_IOCTL_TYPE, 0x24, unsigned long)
#define ACRN_IOCTL_SET_IRQLINE		\
	_IOW(ACRN_IOCTL_TYPE, 0x25, __u64)

/* DM ioreq management */
#define ACRN_IOCTL_NOTIFY_REQUEST_FINISH \
	_IOW(ACRN_IOCTL_TYPE, 0x31, struct acrn_ioreq_notify)
#define ACRN_IOCTL_CREATE_IOREQ_CLIENT	\
	_IO(ACRN_IOCTL_TYPE, 0x32)
#define ACRN_IOCTL_ATTACH_IOREQ_CLIENT	\
	_IO(ACRN_IOCTL_TYPE, 0x33)
#define ACRN_IOCTL_DESTROY_IOREQ_CLIENT	\
	_IO(ACRN_IOCTL_TYPE, 0x34)
#define ACRN_IOCTL_CLEAR_VM_IOREQ	\
	_IO(ACRN_IOCTL_TYPE, 0x35)

/* Guest memory management */
#define ACRN_IOCTL_SET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x41, struct acrn_vm_memmap)
#define ACRN_IOCTL_UNSET_MEMSEG		\
	_IOW(ACRN_IOCTL_TYPE, 0x42, struct acrn_vm_memmap)

/* PCI assignment*/
#define ACRN_IOCTL_SET_PTDEV_INTR	\
	_IOW(ACRN_IOCTL_TYPE, 0x53, struct acrn_ptdev_irq)
#define ACRN_IOCTL_RESET_PTDEV_INTR	\
	_IOW(ACRN_IOCTL_TYPE, 0x54, struct acrn_ptdev_irq)
#define ACRN_IOCTL_ASSIGN_PCIDEV	\
	_IOW(ACRN_IOCTL_TYPE, 0x55, struct acrn_pcidev)
#define ACRN_IOCTL_DEASSIGN_PCIDEV	\
	_IOW(ACRN_IOCTL_TYPE, 0x56, struct acrn_pcidev)
#define ACRN_IOCTL_ASSIGN_MMIODEV	\
	_IOW(ACRN_IOCTL_TYPE, 0x57, struct acrn_mmiodev)
#define ACRN_IOCTL_DEASSIGN_MMIODEV	\
	_IOW(ACRN_IOCTL_TYPE, 0x58, struct acrn_mmiodev)
#define ACRN_IOCTL_CREATE_VDEV	\
	_IOW(ACRN_IOCTL_TYPE, 0x59, struct acrn_vdev)
#define ACRN_IOCTL_DESTROY_VDEV	\
	_IOW(ACRN_IOCTL_TYPE, 0x5A, struct acrn_vdev)

/* Power management */
#define ACRN_IOCTL_PM_GET_CPU_STATE	\
	_IOWR(ACRN_IOCTL_TYPE, 0x60, __u64)

/* HSM eventfd */
#define ACRN_IOCTL_IOEVENTFD		\
	_IOW(ACRN_IOCTL_TYPE, 0x70, struct acrn_ioeventfd)
#define ACRN_IOCTL_IRQFD		\
	_IOW(ACRN_IOCTL_TYPE, 0x71, struct acrn_irqfd)

/* Asynchronous IO */
#define ACRN_IOCTL_SETUP_ASYNCIO	\
	_IOW(ACRN_IOCTL_TYPE, 0x90, __u64)

#define	ACRN_MEM_ACCESS_RIGHT_MASK	0x00000007U
#define	ACRN_MEM_ACCESS_READ		0x00000001U
#define	ACRN_MEM_ACCESS_WRITE		0x00000002U
#define	ACRN_MEM_ACCESS_EXEC		0x00000004U
#define	ACRN_MEM_ACCESS_RWX		(ACRN_MEM_ACCESS_READ  | \
					 ACRN_MEM_ACCESS_WRITE | \
					 ACRN_MEM_ACCESS_EXEC)

#define	ACRN_MEM_TYPE_MASK		0x000007C0U
#define	ACRN_MEM_TYPE_WB		0x00000040U
#define	ACRN_MEM_TYPE_WT		0x00000080U
#define	ACRN_MEM_TYPE_UC		0x00000100U
#define	ACRN_MEM_TYPE_WC		0x00000200U
#define	ACRN_MEM_TYPE_WP		0x00000400U

/* Memory mapping types */
#define	ACRN_MEMMAP_RAM			0
#define	ACRN_MEMMAP_MMIO		1

/**
 * @brief EPT memory mapping info for guest
 */
struct acrn_vm_memmap {
	/** memory mapping type */
	__u32	type;
	/** memory mapping attribute */
	__u32	attr;
	/** user OS guest physical start address of memory mapping */
	__u64	user_vm_pa;
	union {
		/** host physical start address of memory,
		 * only for type == VM_MMIO
		 */
		__u64	service_vm_pa;
		/** service OS user virtual start address of
		 * memory, only for type == VM_MEMMAP_SYSMEM &&
		 * using_vma == true
		 */
		__u64	vma_base;
	};
	/** the length of memory range mapped */
	__u64	len;
};

/* Type of interrupt of a passthrough device */
#define ACRN_PTDEV_IRQ_INTX	0
#define ACRN_PTDEV_IRQ_MSI	1
#define ACRN_PTDEV_IRQ_MSIX	2

/**
 * @brief pass thru device irq data structure
 */
struct acrn_ptdev_irq {
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
struct acrn_ioreq_notify {
	/** VM id to identify ioreq client */
	__u16	vmid;
	__u16	reserved;
	/** identify the ioreq submitter */
	__u32	vcpu;
};

#define ACRN_PLATFORM_LAPIC_IDS_MAX	64
struct acrn_ioeventfd {
#define ACRN_IOEVENTFD_FLAG_PIO		0x01
#define ACRN_IOEVENTFD_FLAG_DATAMATCH	0x02
#define ACRN_IOEVENTFD_FLAG_DEASSIGN	0x04
#define ACRN_IOEVENTFD_FLAG_ASYNCIO	0x08
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
