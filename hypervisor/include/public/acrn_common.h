/*
 * common definition
 *
 * Copyright (C) 2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file acrn_common.h
 *
 * @brief acrn common data structure for hypercall or ioctl
 */

#ifndef ACRN_COMMON_H
#define ACRN_COMMON_H

#include <types.h>

/*
 * Common structures for ACRN/VHM/DM
 */

/*
 * IO request
 */
#define VHM_REQUEST_MAX 16U

#define REQ_STATE_FREE          3
#define REQ_STATE_PENDING	0
#define REQ_STATE_COMPLETE	1
#define REQ_STATE_PROCESSING	2
#define REQ_STATE_FAILED	-1

#define REQ_PORTIO	0U
#define REQ_MMIO	1U
#define REQ_PCICFG	2U
#define REQ_WP		3U

#define REQUEST_READ	0U
#define REQUEST_WRITE	1U

/* IOAPIC device model info */
#define VIOAPIC_RTE_NUM	48U  /* vioapic pins */

#if VIOAPIC_RTE_NUM < 24
#error "VIOAPIC_RTE_NUM must be larger than 23"
#endif

/* Generic VM flags from guest OS */
#define SECURE_WORLD_ENABLED    (1UL<<0)  /* Whether secure world is enabled */

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

struct mmio_request {
	uint32_t direction;
	uint32_t reserved;
	uint64_t address;
	uint64_t size;
	uint64_t value;
} __aligned(8);

struct pio_request {
	uint32_t direction;
	uint32_t reserved;
	uint64_t address;
	uint64_t size;
	uint32_t value;
} __aligned(8);

struct pci_request {
	uint32_t direction;
	uint32_t reserved[3];/* need keep same header fields with pio_request */
	int64_t size;
	int32_t value;
	int32_t bus;
	int32_t dev;
	int32_t func;
	int32_t reg;
} __aligned(8);

union vhm_io_request {
	struct pio_request pio;
	struct pci_request pci;
	struct mmio_request mmio;
	int64_t reserved1[8];
};

/**
 * @brief 256-byte VHM requests
 *
 * The state transitions of a VHM request are:
 *
 *    FREE -> PENDING -> PROCESSING -> COMPLETE -> FREE -> ...
 *                                \              /
 *                                 +--> FAILED -+
 *
 * When a request is in COMPLETE or FREE state, the request is owned by the
 * hypervisor. SOS (VHM or DM) shall not read or write the internals of the
 * request except the state.
 *
 * When a request is in PENDING or PROCESSING state, the request is owned by
 * SOS. The hypervisor shall not read or write the request other than the state.
 *
 * Based on the rules above, a typical VHM request lifecycle should looks like
 * the following.
 *
 *                     (assume the initial state is FREE)
 *
 *       SOS vCPU 0                SOS vCPU x                    UOS vCPU y
 *
 *                                                 hypervisor:
 *                                                     fill in type, addr, etc.
 *                                                     pause UOS vcpu y
 *                                                     set state to PENDING (a)
 *                                                     fire upcall to SOS vCPU 0
 *
 *  VHM:
 *      scan for pending requests
 *      set state to PROCESSING (b)
 *      assign requests to clients (c)
 *
 *                            client:
 *                                scan for assigned requests
 *                                handle the requests (d)
 *                                set state to COMPLETE
 *                                notify the hypervisor
 *
 *                            hypervisor:
 *                                resume UOS vcpu y (e)
 *
 *                                                 hypervisor:
 *                                                     post-work (f)
 *                                                     set state to FREE
 *
 * Note that the following shall hold.
 *
 *   1. (a) happens before (b)
 *   2. (c) happens before (d)
 *   3. (e) happens before (f)
 *   4. One vCPU cannot trigger another I/O request before the previous one has
 *      completed (i.e. the state switched to FREE)
 *
 * Accesses to the state of a vhm_request shall be atomic and proper barriers
 * are needed to ensure that:
 *
 *   1. Setting state to PENDING is the last operation when issuing a request in
 *      the hypervisor, as the hypervisor shall not access the request any more.
 *
 *   2. Due to similar reasons, setting state to COMPLETE is the last operation
 *      of request handling in VHM or clients in SOS.
 *
 * The state FAILED is an obsolete state to indicate that the I/O request cannot
 * be handled. In such cases the mediators and DM should switch the state to
 * COMPLETE with the value set to all 1s for read, and skip the request for
 * writes. This state WILL BE REMOVED after the mediators and DM are updated to
 * follow this rule.
 */
struct vhm_request {
	/**
	 * Type of this request.
	 *
	 * Byte offset: 0.
	 */
	uint32_t type;

	/**
	 * Reserved.
	 *
	 * Byte offset: 4.
	 */
	uint32_t reserved0[15];

	/**
	 * Details about this request. For REQ_PORTIO, this has type
	 * pio_request. For REQ_MMIO and REQ_WP, this has type mmio_request. For
	 * REQ_PCICFG, this has type pci_request.
	 *
	 * Byte offset: 64.
	 */
	union vhm_io_request reqs;

	/**
	 * Whether this request is valid for processing. ACRN write, VHM read
	 * only.
	 *
	 * Warning; this field is obsolete and will be removed soon.
	 *
	 * Byte offset: 128.
	 */
	int32_t valid;

	/**
	 * The client which is distributed to handle this request. Accessed by
	 * VHM only.
	 *
	 * Byte offset: 132.
	 */
	int32_t client;

	/**
	 * The status of this request, taking REQ_STATE_xxx as values.
	 *
	 * Byte offset: 136.
	 */
	int32_t processed;
} __aligned(256);

union vhm_request_buffer {
	struct vhm_request req_queue[VHM_REQUEST_MAX];
	int8_t reserved[4096];
} __aligned(4096);

/**
 * @brief Info to create a VM, the parameter for HC_CREATE_VM hypercall
 */
struct acrn_create_vm {
	/** created vmid return to VHM. Keep it first field */
	uint16_t vmid;

	/** Reserved */
	uint16_t reserved0;

	/** VCPU numbers this VM want to create */
	uint16_t vcpu_num;

	/** Reserved */
	uint16_t reserved1;

	/** the GUID of this VM */
	uint8_t	 GUID[16];

	/* VM flag bits from Guest OS, now used
	 *  SECURE_WORLD_ENABLED          (1UL<<0)
	 */
	uint64_t vm_flag;

	/** Reserved for future use*/
	uint8_t  reserved2[24];
} __aligned(8);

/**
 * @brief Info to create a VCPU
 *
 * the parameter for HC_CREATE_VCPU hypercall
 */
struct acrn_create_vcpu {
	/** the virtual CPU ID for the VCPU created */
	uint16_t vcpu_id;

	/** the physical CPU ID for the VCPU created */
	uint16_t pcpu_id;
} __aligned(8);

/**
 * @brief Info to set ioreq buffer for a created VM
 *
 * the parameter for HC_SET_IOREQ_BUFFER hypercall
 */
struct acrn_set_ioreq_buffer {
	/** guest physical address of VM request_buffer */
	uint64_t req_buf;
} __aligned(8);

/** Interrupt type for acrn_irqline: inject interrupt to IOAPIC */
#define	ACRN_INTR_TYPE_ISA	0U

/** Interrupt type for acrn_irqline: inject interrupt to both PIC and IOAPIC */
#define	ACRN_INTR_TYPE_IOAPIC	1U

/**
 * @brief Info to assert/deassert/pulse a virtual IRQ line for a VM
 *
 * the parameter for HC_ASSERT_IRQLINE/HC_DEASSERT_IRQLINE/HC_PULSE_IRQLINE
 * hypercall
 */
struct acrn_irqline {
	/** interrupt type which could be IOAPIC or ISA */
	uint32_t intr_type;

	/** reserved for alignment padding */
	uint32_t reserved;

	/** pic IRQ for ISA type */
	uint32_t pic_irq;

	/** Reserved */
	uint32_t reserved0;

	/** ioapic IRQ for IOAPIC & ISA TYPE,
	 *  if ~0U then this IRQ will not be injected
	 */
	uint32_t ioapic_irq;

	/** Reserved */
	uint32_t reserved1;
} __aligned(8);

/**
 * @brief Info to inject a MSI interrupt to VM
 *
 * the parameter for HC_INJECT_MSI hypercall
 */
struct acrn_msi_entry {
	/** MSI addr[19:12] with dest VCPU ID */
	uint64_t msi_addr;

	/** MSI data[7:0] with vector */
	uint64_t msi_data;
} __aligned(8);

/**
 * @brief Info to inject a NMI interrupt for a VM
 */
struct acrn_nmi_entry {
	/** virtual CPU ID to inject */
	uint16_t vcpu_id;

	/** Reserved */
	uint16_t reserved0;

	/** Reserved */
	uint32_t reserved1;
} __aligned(8);

/**
 * @brief Info to remap pass-through PCI MSI for a VM
 *
 * the parameter for HC_VM_PCI_MSIX_REMAP hypercall
 */
struct acrn_vm_pci_msix_remap {
	/** pass-through PCI device virtual BDF# */
	uint16_t virt_bdf;

	/** pass-through PCI device physical BDF# */
	uint16_t phys_bdf;

	/** pass-through PCI device MSI/MSI-X cap control data */
	uint16_t msi_ctl;

	/** reserved for alignment padding */
	uint16_t reserved;

	/** pass-through PCI device MSI address to remap, which will
	 * return the caller after remapping
	 */
	uint64_t msi_addr;		/* IN/OUT: msi address to fix */

	/** pass-through PCI device MSI data to remap, which will
	 * return the caller after remapping
	 */
	uint32_t msi_data;

	/** pass-through PCI device is MSI or MSI-X
	 *  0 - MSI, 1 - MSI-X
	 */
	int32_t msix;

	/** if the pass-through PCI device is MSI-X, this field contains
	 *  the MSI-X entry table index
	 */
	uint32_t msix_entry_index;

	/** if the pass-through PCI device is MSI-X, this field contains
	 *  Vector Control for MSI-X Entry, field defined in MSI-X spec
	 */
	uint32_t vector_ctl;
} __aligned(8);

/**
 * @brief The guest config pointer offset.
 *
 * It's designed to support passing DM config data pointer, based on it,
 * hypervisor would parse then pass DM defined configuration to GUEST VCPU
 * when booting guest VM.
 * the address 0xef000 here is designed by DM, as it arranged all memory
 * layout below 1M, DM add this address to E280 reserved range to make sure
 * there is no overlap for the address 0xef000 usage.
 */
#define GUEST_CFG_OFFSET	0xef000UL

/**
 * @brief Info The power state data of a VCPU.
 *
 */

#define SPACE_SYSTEM_MEMORY     0U
#define SPACE_SYSTEM_IO         1U
#define SPACE_PCI_CONFIG        2U
#define SPACE_Embedded_Control  3U
#define SPACE_SMBUS             4U
#define SPACE_PLATFORM_COMM     10U
#define SPACE_FFixedHW          0x7FU

struct acpi_generic_address {
	uint8_t 	space_id;
	uint8_t 	bit_width;
	uint8_t 	bit_offset;
	uint8_t 	access_size;
	uint64_t	address;
} __attribute__((aligned(8)));

struct cpu_cx_data {
	struct acpi_generic_address cx_reg;
	uint8_t 	type;
	uint32_t	latency;
	uint64_t	power;
} __attribute__((aligned(8)));

struct cpu_px_data {
	uint64_t core_frequency;	/* megahertz */
	uint64_t power;			/* milliWatts */
	uint64_t transition_latency;	/* microseconds */
	uint64_t bus_master_latency;	/* microseconds */
	uint64_t control;		/* control value */
	uint64_t status;		/* success indicator */
} __attribute__((aligned(8)));

struct acpi_sx_pkg {
	uint8_t		val_pm1a;
	uint8_t		val_pm1b;
	uint16_t	reserved;
} __attribute__((aligned(8)));

struct pm_s_state_data {
	struct acpi_generic_address pm1a_evt;
	struct acpi_generic_address pm1b_evt;
	struct acpi_generic_address pm1a_cnt;
	struct acpi_generic_address pm1b_cnt;
	struct acpi_sx_pkg s3_pkg;
	struct acpi_sx_pkg s5_pkg;
	uint32_t *wake_vector_32;
	uint64_t *wake_vector_64;
}__attribute__((aligned(8)));

/**
 * @brief Info PM command from DM/VHM.
 *
 * The command would specify request type(e.g. get px count or data) for
 * specific VM and specific VCPU with specific state number.
 * For Px, PMCMD_STATE_NUM means Px number from 0 to (MAX_PSTATE - 1),
 * For Cx, PMCMD_STATE_NUM means Cx entry index from 1 to MAX_CX_ENTRY.
 */
#define PMCMD_VMID_MASK		0xff000000U
#define PMCMD_VCPUID_MASK	0x00ff0000U
#define PMCMD_STATE_NUM_MASK	0x0000ff00U
#define PMCMD_TYPE_MASK		0x000000ffU

#define PMCMD_VMID_SHIFT	24
#define PMCMD_VCPUID_SHIFT	16
#define PMCMD_STATE_NUM_SHIFT	8

enum pm_cmd_type {
	PMCMD_GET_PX_CNT,
	PMCMD_GET_PX_DATA,
	PMCMD_GET_CX_CNT,
	PMCMD_GET_CX_DATA,
};

/**
 * @}
 */
#endif /* ACRN_COMMON_H */
