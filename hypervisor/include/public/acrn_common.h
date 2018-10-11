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

#define REQ_STATE_FREE          3U
#define REQ_STATE_PENDING	0U
#define REQ_STATE_COMPLETE	1U
#define REQ_STATE_PROCESSING	2U

#define REQ_PORTIO	0U
#define REQ_MMIO	1U
#define REQ_PCICFG	2U
#define REQ_WP		3U

#define REQUEST_READ	0U
#define REQUEST_WRITE	1U

/* IOAPIC device model info */
#define VIOAPIC_RTE_NUM	48U  /* vioapic pins */

#if VIOAPIC_RTE_NUM < 24U
#error "VIOAPIC_RTE_NUM must be larger than 23"
#endif

/* Generic VM flags from guest OS */
#define SECURE_WORLD_ENABLED    (1UL << 0U)  /* Whether secure world is enabled */

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Representation of a MMIO request
 */
struct mmio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p REQUEST_READ or \p REQUEST_WRITE.
	 */
	uint32_t direction;

	/**
	 * @brief reserved
	 */
	uint32_t reserved;

	/**
	 * @brief Address of the I/O access
	 */
	uint64_t address;

	/**
	 * @brief Width of the I/O access in byte
	 */
	uint64_t size;

	/**
	 * @brief The value read for I/O reads or to be written for I/O writes
	 */
	uint64_t value;
} __aligned(8);

/**
 * @brief Representation of a port I/O request
 */
struct pio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p REQUEST_READ or \p REQUEST_WRITE.
	 */
	uint32_t direction;

	/**
	 * @brief reserved
	 */
	uint32_t reserved;

	/**
	 * @brief Port address of the I/O access
	 */
	uint64_t address;

	/**
	 * @brief Width of the I/O access in byte
	 */
	uint64_t size;

	/**
	 * @brief The value read for I/O reads or to be written for I/O writes
	 */
	uint32_t value;
} __aligned(8);

/**
 * @brief Representation of a PCI configuration space access
 */
struct pci_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p REQUEST_READ or \p REQUEST_WRITE.
	 */
	uint32_t direction;

	/**
	 * @brief Reserved
	 */
	uint32_t reserved[3];/* need keep same header fields with pio_request */

	/**
	 * @brief Width of the I/O access in byte
	 */
	int64_t size;

	/**
	 * @brief The value read for I/O reads or to be written for I/O writes
	 */
	int32_t value;

	/**
	 * @brief The \p bus part of the BDF of the device
	 */
	int32_t bus;

	/**
	 * @brief The \p device part of the BDF of the device
	 */
	int32_t dev;

	/**
	 * @brief The \p function part of the BDF of the device
	 */
	int32_t func;

	/**
	 * @brief The register to be accessed in the configuration space
	 */
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
 * @verbatim embed:rst:leading-asterisk
 *
 * +-----------------------+-------------------------+----------------------+
 * | SOS vCPU 0            | SOS vCPU x              | UOS vCPU y           |
 * +=======================+=========================+======================+
 * |                       |                         | **Hypervisor**:      |
 * |                       |                         |                      |
 * |                       |                         | - Fill in type,      |
 * |                       |                         |   addr, etc.         |
 * |                       |                         | - Pause UOS vCPU y   |
 * |                       |                         | - Set state to       |
 * |                       |                         |   PENDING **(a)**    |
 * |                       |                         | - Fire upcall to     |
 * |                       |                         |   SOS vCPU 0         |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * | **VHM**:              |                         |                      |
 * |                       |                         |                      |
 * | - Scan for pending    |                         |                      |
 * |   requests            |                         |                      |
 * | - Set state to        |                         |                      |
 * |   PROCESSING **(b)**  |                         |                      |
 * | - Assign requests to  |                         |                      |
 * |   clients **(c)**     |                         |                      |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * |                       | **Client**:             |                      |
 * |                       |                         |                      |
 * |                       | - Scan for assigned     |                      |
 * |                       |   requests              |                      |
 * |                       | - Handle the            |                      |
 * |                       |   requests **(d)**      |                      |
 * |                       | - Set state to COMPLETE |                      |
 * |                       | - Notify the hypervisor |                      |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * |                       | **Hypervisor**:         |                      |
 * |                       |                         |                      |
 * |                       | - resume UOS vCPU y     |                      |
 * |                       |   **(e)**               |                      |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * |                       |                         | **Hypervisor**:      |
 * |                       |                         |                      |
 * |                       |                         | - Post-work **(f)**  |
 * |                       |                         | - set state to FREE  |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 *
 * @endverbatim
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
 */
struct vhm_request {
	/**
	 * Type of this request.
	 *
	 * Byte offset: 0.
	 */
	uint32_t type;

	/**
	 * @brief Hypervisor will poll completion if set.
	 *
	 * Byte offset: 4.
	 */
	uint32_t completion_polling;

	/**
	 * Reserved.
	 *
	 * Byte offset: 8.
	 */
	uint32_t reserved0[14];

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
	uint32_t processed;
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

/* General-purpose register layout aligned with the general-purpose register idx
 * when vmexit, such as vmexit due to CR access, refer to SMD Vol.3C 27-6.
 */
struct acrn_gp_regs {
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};

/* struct to define how the descriptor stored in memory.
 * Refer SDM Vol3 3.5.1 "Segment Descriptor Tables"
 * Figure 3-11
 */
struct acrn_descriptor_ptr {
	uint16_t limit;
	uint64_t base;
	uint16_t reserved[3];   /* align struct size to 64bit */
} __attribute__((packed));

/**
 * @brief registers info for vcpu.
 */
struct acrn_vcpu_regs {
	struct acrn_gp_regs gprs;
	struct acrn_descriptor_ptr gdt;
	struct acrn_descriptor_ptr idt;

	uint64_t        rip;
	uint64_t        cs_base;
	uint64_t        cr0;
	uint64_t        cr4;
	uint64_t        cr3;
	uint64_t        ia32_efer;
	uint64_t        rflags;
	uint64_t        reserved_64[4];

	uint32_t        cs_ar;
	uint32_t        cs_limit;
	uint32_t        reserved_32[3];

	/* don't change the order of following sel */
	uint16_t        cs_sel;
	uint16_t        ss_sel;
	uint16_t        ds_sel;
	uint16_t        es_sel;
	uint16_t        fs_sel;
	uint16_t        gs_sel;
	uint16_t        ldt_sel;
	uint16_t        tr_sel;

	uint16_t        reserved_16[4];
};

/**
 * @brief Info to set vcpu state
 *
 * the pamameter for HC_SET_VCPU_STATE
 */
struct acrn_set_vcpu_regs {
	/** the virtual CPU ID for the VCPU to set state */
	uint16_t vcpu_id;

	/** reserved space to make cpu_state aligned to 8 bytes */
	uint16_t reserved0[3];

	/** the structure to hold vcpu state */
	struct acrn_vcpu_regs vcpu_regs;
} __attribute__((aligned(8)));

/**
 * @brief Info to set ioreq buffer for a created VM
 *
 * the parameter for HC_SET_IOREQ_BUFFER hypercall
 */
struct acrn_set_ioreq_buffer {
	/** guest physical address of VM request_buffer */
	uint64_t req_buf;
} __aligned(8);

/** Operation types for setting IRQ line */
#define GSI_SET_HIGH		0U
#define GSI_SET_LOW		1U
#define GSI_RAISING_PULSE	2U
#define GSI_FALLING_PULSE	3U

/**
 * @brief Info to Set/Clear/Pulse a virtual IRQ line for a VM
 *
 * the parameter for HC_SET_IRQLINE hypercall
 */
struct acrn_irqline_ops {
	uint32_t nr_gsi;
	uint32_t op;
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

#define PMCMD_VMID_SHIFT	24U
#define PMCMD_VCPUID_SHIFT	16U
#define PMCMD_STATE_NUM_SHIFT	8U

enum pm_cmd_type {
	PMCMD_GET_PX_CNT,
	PMCMD_GET_PX_DATA,
	PMCMD_GET_CX_CNT,
	PMCMD_GET_CX_DATA,
};

/**
 * @brief Info to get a VM interrupt count data
 *
 * the parameter for HC_VM_INTR_MONITOR hypercall
 */
#define MAX_PTDEV_NUM 24
struct acrn_intr_monitor {
	/** sub command for intr monitor */
	uint32_t cmd;
	/** the count of this buffer to save */
	uint32_t buf_cnt;

	/** the buffer which save each interrupt count */
	uint64_t buffer[MAX_PTDEV_NUM * 2];
} __aligned(8);

/** cmd for intr monitor **/
#define INTR_CMD_GET_DATA 0U
#define INTR_CMD_DELAY_INT 1U

/**
 * @}
 */
#endif /* ACRN_COMMON_H */
