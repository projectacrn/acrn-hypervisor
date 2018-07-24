/*
 * hypercall definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file acrn_hv_defs.h
 *
 * @brief acrn data structure for hypercall
 */

#ifndef ACRN_HV_DEFS_H
#define ACRN_HV_DEFS_H

/*
 * Common structures for HV/VHM
 */

#define _HC_ID(x, y) (((x)<<24)|(y))

#define HC_ID 0x80UL

/* general */
#define HC_ID_GEN_BASE               0x0UL
#define HC_GET_API_VERSION          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x00UL)
#define HC_SOS_OFFLINE_CPU          _HC_ID(HC_ID, HC_ID_GEN_BASE + 0x01UL)

/* VM management */
#define HC_ID_VM_BASE               0x10UL
#define HC_CREATE_VM                _HC_ID(HC_ID, HC_ID_VM_BASE + 0x00UL)
#define HC_DESTROY_VM               _HC_ID(HC_ID, HC_ID_VM_BASE + 0x01UL)
#define HC_START_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x02UL)
#define HC_PAUSE_VM                 _HC_ID(HC_ID, HC_ID_VM_BASE + 0x03UL)
#define HC_CREATE_VCPU              _HC_ID(HC_ID, HC_ID_VM_BASE + 0x04UL)

/* IRQ and Interrupts */
#define HC_ID_IRQ_BASE              0x20UL
#define HC_ASSERT_IRQLINE           _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x00UL)
#define HC_DEASSERT_IRQLINE         _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x01UL)
#define HC_PULSE_IRQLINE            _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x02UL)
#define HC_INJECT_MSI               _HC_ID(HC_ID, HC_ID_IRQ_BASE + 0x03UL)

/* DM ioreq management */
#define HC_ID_IOREQ_BASE            0x30UL
#define HC_SET_IOREQ_BUFFER         _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x00UL)
#define HC_NOTIFY_REQUEST_FINISH    _HC_ID(HC_ID, HC_ID_IOREQ_BASE + 0x01UL)

/* Guest memory management */
#define HC_ID_MEM_BASE              0x40UL
#define HC_VM_SET_MEMORY_REGION     _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x00UL)
#define HC_VM_GPA2HPA               _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x01UL)
#define HC_VM_SET_MEMORY_REGIONS    _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x02UL)
#define HC_VM_WRITE_PROTECT_PAGE    _HC_ID(HC_ID, HC_ID_MEM_BASE + 0x03UL)

/* PCI assignment*/
#define HC_ID_PCI_BASE              0x50UL
#define HC_ASSIGN_PTDEV             _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x00UL)
#define HC_DEASSIGN_PTDEV           _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x01UL)
#define HC_VM_PCI_MSIX_REMAP        _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x02UL)
#define HC_SET_PTDEV_INTR_INFO      _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x03UL)
#define HC_RESET_PTDEV_INTR_INFO    _HC_ID(HC_ID, HC_ID_PCI_BASE + 0x04UL)

/* DEBUG */
#define HC_ID_DBG_BASE              0x60UL
#define HC_SETUP_SBUF               _HC_ID(HC_ID, HC_ID_DBG_BASE + 0x00UL)

/* Trusty */
#define HC_ID_TRUSTY_BASE           0x70UL
#define HC_INITIALIZE_TRUSTY        _HC_ID(HC_ID, HC_ID_TRUSTY_BASE + 0x00UL)
#define HC_WORLD_SWITCH             _HC_ID(HC_ID, HC_ID_TRUSTY_BASE + 0x01UL)
#define HC_GET_SEC_INFO             _HC_ID(HC_ID, HC_ID_TRUSTY_BASE + 0x02UL)

/* Power management */
#define HC_ID_PM_BASE               0x80UL
#define HC_PM_GET_CPU_STATE         _HC_ID(HC_ID, HC_ID_PM_BASE + 0x00UL)

#define ACRN_DOM0_VMID (0UL)
#define ACRN_INVALID_VMID (0xffffU)
#define ACRN_INVALID_HPA (~0UL)

/* Generic memory attributes */
#define	MEM_ACCESS_READ                 0x00000001U
#define	MEM_ACCESS_WRITE                0x00000002U
#define	MEM_ACCESS_EXEC	                0x00000004U
#define	MEM_ACCESS_RWX			(MEM_ACCESS_READ | MEM_ACCESS_WRITE | \
						MEM_ACCESS_EXEC)
#define MEM_ACCESS_RIGHT_MASK           0x00000007U
#define	MEM_TYPE_WB                     0x00000040U
#define	MEM_TYPE_WT                     0x00000080U
#define	MEM_TYPE_UC                     0x00000100U
#define	MEM_TYPE_WC                     0x00000200U
#define	MEM_TYPE_WP                     0x00000400U
#define MEM_TYPE_MASK                   0x000007C0U

/**
 * @brief Hypercall
 *
 * @defgroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Info to set guest memory region mapping
 *
 * the parameter for HC_VM_SET_MEMORY_REGION hypercall
 */
struct vm_memory_region {
#define MR_ADD		0U
#define MR_DEL		2U
#define MR_MODIFY	3U
	/** set memory region type: MR_ADD or MAP_DEL */
	uint32_t type;

	/** memory attributes: memory type + RWX access right */
	uint32_t prot;

	/** the beginning guest physical address of the memory reion*/
	uint64_t gpa;

	/** VM0's guest physcial address which gpa will be mapped to */
	uint64_t vm0_gpa;

	/** size of the memory region */
	uint64_t size;
} __aligned(8);

/**
 * set multi memory regions, used for HC_VM_SET_MEMORY_REGIONS
 */
struct set_regions {
	/** vmid for this hypercall */
	uint16_t vmid;

	/** Reserved */
	uint16_t reserved0;

	/** Reserved */
	uint32_t reserved1;

	/**  memory region numbers */
	uint32_t mr_num;

	/** the gpa of regions buffer, point to the regions array:
	 *  	struct vm_memory_region regions[mr_num]
	 * the max buffer size is one page.
	 */
	uint64_t regions_gpa;
} __attribute__((aligned(8)));

/**
 * @brief Info to change guest one page write protect permission
 *
 * the parameter for HC_VM_WRITE_PROTECT_PAGE hypercall
 */
struct wp_data {
	/** set page write protect permission.
	 *  ture: set the wp; flase: clear the wp
	 */
	uint8_t set;

	/** Reserved */
	uint64_t pad:56;

	/** the guest physical address of the page to change */
	uint64_t gpa;
} __aligned(8);

/**
 * Setup parameter for share buffer, used for HC_SETUP_SBUF hypercall
 */
struct sbuf_setup_param {
	/** sbuf physical cpu id */
	uint16_t pcpu_id;

	/** Reserved */
	uint16_t reserved;

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
#define IRQ_INTX 0U
#define IRQ_MSI 1U
#define IRQ_MSIX 2U
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
			uint8_t virt_pin;

			/** Reserved */
			uint32_t reserved0:24;

			/** physical IOAPIC pin */
			uint8_t phys_pin;

			/** Reserved */
			uint32_t reserved1:24;

			/** is virtual pin from PIC */
			bool pic_pin;

			/** Reserved */
			uint32_t reserved2:24;
		} intx;

		/** MSIx remapping info */
		struct {
			/** vector count of MSI/MSIX */
			uint32_t vector_cnt;
		} msix;
	} is;	/* irq source */
} __aligned(8);

/**
 * Hypervisor api version info, return it for HC_GET_API_VERSION hypercall
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

	/** padding */
	uint32_t padding;

	/** trusty runtime memory base address (high 32bit) */
	uint32_t base_addr_high;

	/** trusty entry point (high 32bit) */
	uint32_t entry_point_high;

	/** rpmb key */
	uint8_t rpmb_key[64];
} __aligned(8);

/**
 * @}
 */

#endif /* ACRN_HV_DEFS_H */
