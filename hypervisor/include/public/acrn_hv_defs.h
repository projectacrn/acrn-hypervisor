/*
 * hypercall definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file acrn_hv_defs.h
 *
 * @brief acrn data structure for hypercall
 */

#ifndef ACRN_HV_DEFS_H
#define ACRN_HV_DEFS_H

/*
 * Common structures for ACRN/VHM/DM
 */
#include "acrn_common.h"

/*
 * Common structures for HV/VHM
 */

#define _HC_ID(x, y) (((x)<<24)|(y))

#define HC_ID 0x80UL

/* general */
#define HC_ID_GEN_BASE               0x0UL
#define HC_GET_API_VERSION          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00)

/* VM management */
#define HC_ID_VM_BASE               0x10UL
#define HC_CREATE_VM                _HC_ID(HC_ID, HC_ID_VM_BASE + 0x00)
#define HC_DESTROY_VM               _HC_ID(HC_ID, HC_ID_VM_BASE + 0x01)
#define HC_START_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x02)
#define HC_PAUSE_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x03)
#define HC_CREATE_VCPU              _HC_ID(HC_ID, HC_ID_VM_BASE + 0x04)

/* IRQ and Interrupts */
#define HC_ID_IRQ_BASE              0x20UL
#define HC_ASSERT_IRQLINE           _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x00)
#define HC_DEASSERT_IRQLINE         _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x01)
#define HC_PULSE_IRQLINE            _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x02)
#define HC_INJECT_MSI               _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03)

/* DM ioreq management */
#define HC_ID_IOREQ_BASE            0x30UL
#define HC_SET_IOREQ_BUFFER         _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00)
#define HC_NOTIFY_REQUEST_FINISH    _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01)

/* Guest memory management */
#define HC_ID_MEM_BASE              0x40UL
#define HC_VM_SET_MEMMAP            _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x00)
#define HC_VM_GPA2HPA               _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x01)
#define HC_VM_SET_MEMMAPS           _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02)

/* PCI assignment*/
#define HC_ID_PCI_BASE              0x50UL
#define HC_ASSIGN_PTDEV             _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x00)
#define HC_DEASSIGN_PTDEV           _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x01)
#define HC_VM_PCI_MSIX_REMAP        _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x02)
#define HC_SET_PTDEV_INTR_INFO      _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03)
#define HC_RESET_PTDEV_INTR_INFO    _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04)

/* DEBUG */
#define HC_ID_DBG_BASE              0x60UL
#define HC_SETUP_SBUF               _HC_ID(HC_ID, HC_ID_DBG_BASE + 0x00)

/* Trusty */
#define HC_ID_TRUSTY_BASE           0x70UL
#define HC_INITIALIZE_TRUSTY        _HC_ID(HC_ID, HC_ID_TRUSTY_BASE + 0x00)
#define HC_WORLD_SWITCH             _HC_ID(HC_ID, HC_ID_TRUSTY_BASE + 0x01)
#define HC_GET_SEC_INFO             _HC_ID(HC_ID, HC_ID_TRUSTY_BASE + 0x02)

/* Power management */
#define HC_ID_PM_BASE               0x80UL
#define HC_PM_GET_CPU_STATE         _HC_ID(HC_ID, HC_ID_PM_BASE + 0x00)

#define ACRN_DOM0_VMID (0UL)
#define ACRN_INVALID_VMID (-1)
#define ACRN_INVALID_HPA (-1UL)

/* Generic memory attributes */
#define	MEM_ACCESS_READ                 0x00000001
#define	MEM_ACCESS_WRITE                0x00000002
#define	MEM_ACCESS_EXEC	                0x00000004
#define	MEM_ACCESS_RWX			(MEM_ACCESS_READ | MEM_ACCESS_WRITE | \
						MEM_ACCESS_EXEC)
#define MEM_ACCESS_RIGHT_MASK           0x00000007
#define	MEM_TYPE_WB                     0x00000040
#define	MEM_TYPE_WT                     0x00000080
#define	MEM_TYPE_UC                     0x00000100
#define	MEM_TYPE_WC                     0x00000200
#define	MEM_TYPE_WP                     0x00000400
#define MEM_TYPE_MASK                   0x000007C0

/**
 * @brief Hypercall
 *
 * @defgroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Info to set ept mapping
 *
 * the parameter for HC_VM_SET_MEMMAP hypercall
 */
struct vm_set_memmap {
#define MAP_MEM		0
#define MAP_MMIO	1
#define MAP_UNMAP	2
	/** map type: MAP_MEM, MAP_MMIO or MAP_UNMAP */
	uint32_t type;

	/** memory attributes: memory type + RWX access right */
	uint32_t prot;

	/** guest physical address to map */
	uint64_t remote_gpa;

	/** VM0's guest physcial address which remote gpa will be mapped to */
	uint64_t vm0_gpa;

	/** length of the map range */
	uint64_t length;

	/** old memory attributes(will be removed in the future):
	 * memory type + RWX access right */
	uint32_t prot_2;
} __aligned(8);

struct memory_map {
	/** map type: MAP_MEM, MAP_MMIO or MAP_UNMAP */
	uint32_t type;

	/** memory attributes: memory type + RWX access right */
	uint32_t prot;

	/** guest physical address to map */
	uint64_t remote_gpa;

	/** VM0's guest physcial address which remote gpa will be mapped to */
	uint64_t vm0_gpa;

	/** length of the map range */
	uint64_t length;

} __aligned(8);

/**
 * multi memmap regions hypercall, used for HC_VM_SET_MEMMAPS
 */
struct set_memmaps {
	/** vmid for this hypercall */
	uint64_t vmid;

	/**  multi memmaps numbers */
	uint32_t memmaps_num;

	/** the gpa of memmaps buffer, point to the memmaps array:
	 *  	struct memory_map regions[memmaps_num]
	 * the max buffer size is one page.
	 */
	uint64_t memmaps_gpa;
} __attribute__((aligned(8)));

/**
 * Setup parameter for share buffer, used for HC_SETUP_SBUF hypercall
 */
struct sbuf_setup_param {
	/** sbuf physical cpu id */
	uint32_t pcpu_id;

	/** sbuf id */
	uint32_t sbuf_id;

	/** sbuf's guest physical address */
	uint64_t gpa;
} __aligned(8);

/**
 * Gpa to hpa translation parameter, used for HC_VM_GPA2HPA hypercall
 */
struct vm_gpa2hpa {
	/** gpa to do translation */
	uint64_t gpa;

	/** hpa to return after translation */
	uint64_t hpa;
} __aligned(8);

/**
 * Intr mapping info per ptdev, the parameter for HC_SET_PTDEV_INTR_INFO
 * hypercall
 */
struct hc_ptdev_irq {
#define IRQ_INTX 0
#define IRQ_MSI 1
#define IRQ_MSIX 2
	/** irq mapping type: INTX or MSI */
	uint32_t type;

	/** virtual BDF of the ptdev */
	uint16_t virt_bdf;

	/** physical BDF of the ptdev */
	uint16_t phys_bdf;

	union {
		/** INTX remapping info */
		struct {
			/** virtual IOAPIC/PIC pin */
			uint32_t virt_pin;

			/** physical IOAPIC pin */
			uint32_t phys_pin;

			/** is virtual pin from PIC */
			uint32_t pic_pin;
		} intx;

		/** MSIx remapping info */
		struct {
			/** vector count of MSI/MSIX */
			uint32_t vector_cnt;
		} msix;
	} is;	/* irq source */
} __aligned(8);

/**
 * Hypervisor api version info, return it for HC_GET_API_VERSION hyercall
 */
struct hc_api_version {
	/** hypervisor api major version */
	uint32_t major_version;

	/** hypervisor api minor version */
	uint32_t minor_version;
} __aligned(8);

/**
 * Trusty boot params, used for HC_INITIALIZE_TRUSTY
 */
struct trusty_boot_param {
	/** sizeof this structure */
	uint32_t size_of_this_struct;

	/** version of this structure */
	uint32_t version;

	/** trusty runtime memory base address */
	uint32_t base_addr;

	/** trusty entry point */
	uint32_t entry_point;

	/** trusty runtime memory size */
	uint32_t mem_size;
} __aligned(8);

/**
 * @}
 */

#endif /* ACRN_HV_DEFS_H */
