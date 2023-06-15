/*
 * common definition
 *
 * Copyright (C) 2017-2022 Intel Corporation.
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
 * Common structures for ACRN/HSM/DM
 */

/*
 * IO request
 */

#define ACRN_IO_REQUEST_MAX		16U
#define ACRN_ASYNCIO_MAX		64U

#define ACRN_IOREQ_STATE_PENDING	0U
#define ACRN_IOREQ_STATE_COMPLETE	1U
#define ACRN_IOREQ_STATE_PROCESSING	2U
#define ACRN_IOREQ_STATE_FREE		3U

#define ACRN_IOREQ_TYPE_PORTIO		0U
#define ACRN_IOREQ_TYPE_MMIO		1U
#define ACRN_IOREQ_TYPE_PCICFG		2U
#define ACRN_IOREQ_TYPE_WP		3U

#define ACRN_IOREQ_DIR_READ		0U
#define ACRN_IOREQ_DIR_WRITE		1U



/* IOAPIC device model info */
#define VIOAPIC_RTE_NUM	48U  /* vioapic pins */

#if VIOAPIC_RTE_NUM < 24U
#error "VIOAPIC_RTE_NUM must be larger than 23"
#endif

/* Generic VM flags from guest OS */
#define GUEST_FLAG_SECURE_WORLD_ENABLED		(1UL << 0U)	/* Whether secure world is enabled */
#define GUEST_FLAG_LAPIC_PASSTHROUGH		(1UL << 1U)  	/* Whether LAPIC is passed through */
#define GUEST_FLAG_IO_COMPLETION_POLLING	(1UL << 2U)  	/* Whether need hypervisor poll IO completion */
#define GUEST_FLAG_HIDE_MTRR			(1UL << 3U)  	/* Whether hide MTRR from VM */
#define GUEST_FLAG_RT				(1UL << 4U)     /* Whether the vm is RT-VM */
#define GUEST_FLAG_NVMX_ENABLED			(1UL << 5U)	/* Whether this VM supports nested virtualization */
#define GUEST_FLAG_SECURITY_VM			(1UL << 6U)	/* Whether this VM needs to do security-vm related fixup (TPM2 and SMBIOS pt) */
#define GUEST_FLAG_VCAT_ENABLED			(1UL << 7U)	/* Whether this VM supports vCAT */
#define GUEST_FLAG_STATIC_VM       (1UL << 8U)  /* Whether this VM uses static VM configuration */
#define GUEST_FLAG_TEE				(1UL << 9U)	/* Whether the VM is TEE VM */
#define GUEST_FLAG_REE				(1UL << 10U)	/* Whether the VM is REE VM */
#define GUEST_FLAG_PMU_PASSTHROUGH	(1UL << 11U)    /* Whether PMU is passed through */
#define GUEST_FLAG_VHWP				(1UL << 12U)    /* Whether the VM supports vHWP */
#define GUEST_FLAG_VTM				(1UL << 13U)    /* Whether the VM supports virtual thermal monitor */

/* TODO: We may need to get this addr from guest ACPI instead of hardcode here */
#define VIRTUAL_SLEEP_CTL_ADDR		0x400U /* Pre-launched VM uses ACPI reduced HW mode and sleep control register */
#define VIRTUAL_PM1A_CNT_ADDR		0x404U
#define	VIRTUAL_PM1A_SCI_EN		0x0001
#define VIRTUAL_PM1A_SLP_TYP		0x1c00U
#define VIRTUAL_PM1A_SLP_EN		0x2000U
#define	VIRTUAL_PM1A_ALWAYS_ZERO	0xc003

#define MAX_VM_NAME_LEN        (16U)

/**
 * @brief Hypercall
 *
 * @addtogroup acrn_hypercall ACRN Hypercall
 * @{
 */

/**
 * @brief Representation of a MMIO request
 */
struct acrn_mmio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p ACRN_IOREQ_DIR_READ or \p ACRN_IOREQ_DIR_WRITE.
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
};

/**
 * @brief Representation of a port I/O request
 */
struct acrn_pio_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p ACRN_IOREQ_DIR_READ or \p ACRN_IOREQ_DIR_WRITE.
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
};

/**
 * @brief Representation of a PCI configuration space access
 */
struct acrn_pci_request {
	/**
	 * @brief Direction of the access
	 *
	 * Either \p ACRN_IOREQ_DIR_READ or \p ACRN_IOREQ_DIR_WRITE.
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
};

/**
 * @brief 256-byte I/O requests
 *
 * The state transitions of a I/O request are:
 *
 *    FREE -> PENDING -> PROCESSING -> COMPLETE -> FREE -> ...
 *
 * When a request is in COMPLETE or FREE state, the request is owned by the
 * hypervisor. Service VM (HSM or DM) shall not read or write the internals of the
 * request except the state.
 *
 * When a request is in PENDING or PROCESSING state, the request is owned by
 * Service VM. The hypervisor shall not read or write the request other than the state.
 *
 * Based on the rules above, a typical I/O request lifecycle should looks like
 * the following.
 *
 * @verbatim embed:rst:leading-asterisk
 *
 * +-----------------------+-------------------------+----------------------+
 * | Service VM vCPU 0     | Service VM vCPU x       | User VM vCPU y       |
 * +=======================+=========================+======================+
 * |                       |                         | Hypervisor:          |
 * |                       |                         |                      |
 * |                       |                         | - Fill in type,      |
 * |                       |                         |   addr, etc.         |
 * |                       |                         | - Pause User VM vCPU |
 * |                       |                         | - Set state to       |
 * |                       |                         |   PENDING (a)        |
 * |                       |                         | - Fire upcall to     |
 * |                       |                         |   Service VM vCPU 0  |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * | HSM:                  |                         |                      |
 * |                       |                         |                      |
 * | - Scan for pending    |                         |                      |
 * |   requests            |                         |                      |
 * | - Set state to        |                         |                      |
 * |   PROCESSING (b)      |                         |                      |
 * | - Assign requests to  |                         |                      |
 * |   clients (c)         |                         |                      |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * |                       | Client:                 |                      |
 * |                       |                         |                      |
 * |                       | - Scan for assigned     |                      |
 * |                       |   requests              |                      |
 * |                       | - Handle the            |                      |
 * |                       |   requests (d)          |                      |
 * |                       | - Set state to COMPLETE |                      |
 * |                       | - Notify the hypervisor |                      |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * |                       | Hypervisor:             |                      |
 * |                       |                         |                      |
 * |                       | - resume User VM vCPU y |                      |
 * |                       |   (e)                   |                      |
 * |                       |                         |                      |
 * +-----------------------+-------------------------+----------------------+
 * |                       |                         | Hypervisor:          |
 * |                       |                         |                      |
 * |                       |                         | - Post-work (f)      |
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
 * Accesses to the state of a acrn_io_request shall be atomic and proper barriers
 * are needed to ensure that:
 *
 *   1. Setting state to PENDING is the last operation when issuing a request in
 *      the hypervisor, as the hypervisor shall not access the request any more.
 *
 *   2. Due to similar reasons, setting state to COMPLETE is the last operation
 *      of request handling in HSM or clients in Service VM.
 */
struct acrn_io_request {
	/**
	 * @brief Type of this request.
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
	 * @brief Reserved.
	 *
	 * Byte offset: 8.
	 */
	uint32_t reserved0[14];

	/**
	 * @brief Details about this request.
	 *
	 * Byte offset: 64.
	 */
	union {
		struct acrn_pio_request		pio_request;
		struct acrn_pci_request		pci_request;
		struct acrn_mmio_request	mmio_request;
		uint64_t			data[8];
	} reqs;

	/**
	 * @brief Reserved.
	 *
	 * Byte offset: 128.
	 */
	uint32_t reserved1;

	/**
	 * @brief If this request has been handled by HSM driver.
	 *
	 * Byte offset: 132.
	 */
	int32_t kernel_handled;

	/**
	 * @brief The status of this request.
	 *
	 * Taking ACRN_IOREQ_STATE_xxx as values.
	 *
	 * Byte offset: 136.
	 */
	uint32_t processed;
} __aligned(256);

struct acrn_io_request_buffer {
	union {
		struct acrn_io_request	req_slot[ACRN_IO_REQUEST_MAX];
		int8_t			reserved[4096];
	};
};

struct acrn_asyncio_info {
	uint32_t type;
	uint64_t addr;
	uint64_t fd;
};

/**
 * @brief Info to create a VM, the parameter for HC_CREATE_VM hypercall
 */
struct acrn_vm_creation {
	/** created vmid return to HSM. Keep it first field */
	uint16_t vmid;

	/** Reserved */
	uint16_t reserved0;

	/** VCPU numbers this VM want to create */
	uint16_t vcpu_num;

	/** Reserved */
	uint16_t reserved1;

	/** the name of this VM */
	uint8_t	 name[MAX_VM_NAME_LEN];

	/* VM flag bits from Guest OS, now used
	 *  GUEST_FLAG_SECURE_WORLD_ENABLED          (1UL<<0)
	 */
	uint64_t vm_flag;

	uint64_t ioreq_buf;

	/**
	 *   The least significant set bit is the PCPU # the VCPU 0 maps to;
	 *   second set least significant bit is the PCPU # the VCPU 1 maps to;
	 *   and so on...
	*/
	uint64_t cpu_affinity;
};

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
} __packed;

/**
 * @brief registers info for vcpu.
 */
struct acrn_regs {
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
};

/**
 * @brief Info to set vcpu state
 *
 * the parameter for HC_SET_VCPU_STATE
 */
struct acrn_vcpu_regs {
	/** the virtual CPU ID for the VCPU to set state */
	uint16_t vcpu_id;

	/** reserved space to make cpu_state aligned to 8 bytes */
	uint16_t reserved[3];

	/** the structure to hold vcpu state */
	struct acrn_regs vcpu_regs;
};

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
	uint32_t gsi;
	uint32_t op;
};


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
};

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

struct acrn_acpi_generic_address {
	uint8_t 	space_id;
	uint8_t 	bit_width;
	uint8_t 	bit_offset;
	uint8_t 	access_size;
	uint64_t	address;
} __attribute__ ((__packed__));

struct acrn_cstate_data {
	struct acrn_acpi_generic_address cx_reg;
	uint8_t 	type;
	uint32_t	latency;
	uint64_t	power;
};

struct acrn_pstate_data {
	uint64_t core_frequency;	/* megahertz */
	uint64_t power;			/* milliWatts */
	uint64_t transition_latency;	/* microseconds */
	uint64_t bus_master_latency;	/* microseconds */
	uint64_t control;		/* control value */
	uint64_t status;		/* success indicator */
};

enum acrn_cpufreq_policy_type {
	CPUFREQ_POLICY_PERFORMANCE,
	CPUFREQ_POLICY_NOMINAL,
};

struct acrn_cpufreq_limits {
	/* Performance levels for HWP */
	uint8_t guaranteed_hwp_lvl;
	uint8_t highest_hwp_lvl;
	uint8_t lowest_hwp_lvl;
	/* Index for the p-state table _PSS */
	uint8_t nominal_pstate;
	uint8_t performance_pstate;
};

struct acpi_sx_pkg {
	uint8_t		val_pm1a;
	uint8_t		val_pm1b;
	uint16_t	reserved;
} __aligned(8);

struct pm_s_state_data {
	struct acrn_acpi_generic_address pm1a_evt;
	struct acrn_acpi_generic_address pm1b_evt;
	struct acrn_acpi_generic_address pm1a_cnt;
	struct acrn_acpi_generic_address pm1b_cnt;
	struct acpi_sx_pkg s3_pkg;
	struct acpi_sx_pkg s5_pkg;
	uint32_t *wake_vector_32;
	uint64_t *wake_vector_64;
} __aligned(8);

/**
 * @brief Info PM command from DM/HSM.
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

enum acrn_pm_cmd_type {
	ACRN_PMCMD_GET_PX_CNT,
	ACRN_PMCMD_GET_PX_DATA,
	ACRN_PMCMD_GET_CX_CNT,
	ACRN_PMCMD_GET_CX_DATA,
};

/**
 * @brief Info to get a VM interrupt count data
 *
 * the parameter for HC_VM_INTR_MONITOR hypercall
 */
#define MAX_PTDEV_NUM 24U
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

/*
 * PRE_LAUNCHED_VM is launched by ACRN hypervisor, with LAPIC_PT;
 * Service VM is launched by ACRN hypervisor, without LAPIC_PT;
 * POST_LAUNCHED_VM is launched by ACRN devicemodel, with/without LAPIC_PT depends on usecases.
 *
 * Assumption: vm_configs array is completely initialized w.r.t. load_order member of
 * 		acrn_vm_config for all the VMs.
 */
enum acrn_vm_load_order {
	PRE_LAUNCHED_VM = 0,
	SERVICE_VM,
	POST_LAUNCHED_VM,	/* Launched by Devicemodel in Service VM */
	MAX_LOAD_ORDER
};

struct acrn_vm_config_header {
       enum acrn_vm_load_order load_order;
       char name[MAX_VM_NAME_LEN];
       uint8_t reserved[2];
       uint8_t severity;
       uint64_t cpu_affinity;
       uint64_t guest_flags;
       /*
        * The following are hv-specific members and are thus opaque.
        * vm_config_entry_size determines the real size of this structure.
        */
} __aligned(8);

/**
 * @brief Info to configure virtual root port
 *
 * Configuration passed to hv when adding a virtual root port which
 * is used as PTM root
 */
struct vrp_config
{
	uint16_t phy_bdf;
	uint8_t max_payload; /* dev cap's max payload */
	uint8_t primary_bus;
	uint8_t secondary_bus;
	uint8_t subordinate_bus;
	uint8_t ptm_capable;
	uint32_t ptm_cap_offset;
};

/* Type of PCI device assignment */
#define ACRN_PTDEV_QUIRK_ASSIGN	(1U << 0)

#define ACRN_PCI_NUM_BARS	6U
/**
 * @brief Info to assign or deassign PCI for a VM
 *
 */
struct acrn_pcidev {
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
	uint32_t bar[ACRN_PCI_NUM_BARS];
};

#define MMIODEV_RES_NUM        3

/**
 * @brief Info to assign or deassign a MMIO device for a VM
 */
struct acrn_mmiodev {
	char name[8];
	struct acrn_mmiores {
		/** the gpa of the MMIO region for the MMIO device */
		uint64_t user_vm_pa;
		/** the hpa of the MMIO region for the MMIO device: for post-launched VM
		  * it's pa in service vm; for pre-launched VM it's pa in HV.
		  */
		uint64_t host_pa;
		/** the size of the MMIO region for the MMIO resource */
		uint64_t size;
		/** the memory type of the MMIO region for the MMIO resource */
		uint64_t mem_type;
	} res[MMIODEV_RES_NUM];
};

/**
 * @brief Info to create or destroy a virtual PCI or legacy device for a VM
 *
 * the parameter for HC_CREATE_VDEV or HC_DESTROY_VDEV hypercall
 */
struct acrn_vdev {
	/*
	 * the identifier of the device, the low 32 bits represent the vendor
	 * id and device id of PCI device and the high 32 bits represent the
	 * device number of the legacy device
	 */
	union {
		uint64_t value;
		struct {
			uint16_t vendor;
			uint16_t device;
			uint32_t legacy_id;
		} fields;
	} id;

	/*
	 * the slot of the device, if the device is a PCI device, the slot
	 * represents BDF, otherwise it represents legacy device slot number
	 */
	uint64_t slot;

	/** the IO resource address of the device, initialized by ACRN-DM. */
	uint32_t io_addr[ACRN_PCI_NUM_BARS];

	/** the IO resource size of the device, initialized by ACRN-DM. */
	uint32_t io_size[ACRN_PCI_NUM_BARS];

	/** the options for the virtual device, initialized by ACRN-DM. */
	uint8_t	args[128];
};

#define ACRN_ASYNCIO_PIO	(0x01U)
#define ACRN_ASYNCIO_MMIO	(0x02U)

#define SBUF_MAGIC	0x5aa57aa71aa13aa3UL
#define SBUF_MAX_SIZE	(1UL << 22U)
#define SBUF_HEAD_SIZE	64U

/* sbuf flags */
#define OVERRUN_CNT_EN	(1U << 0U) /* whether overrun counting is enabled */
#define OVERWRITE_EN	(1U << 1U) /* whether overwrite is enabled */

/**
 * (sbuf) head + buf (store (ele_num - 1) elements at most)
 * buffer empty: tail == head
 * buffer full:  (tail + ele_size) % size == head
 *
 *             Base of memory for elements
 *                |
 *                |
 * ----------------------------------------------------------------------
 * | struct shared_buf | raw data (ele_size)| ... | raw data (ele_size) |
 * ----------------------------------------------------------------------
 * |
 * |
 * struct shared_buf *buf
 */

enum {
	ACRN_TRACE = 0U,
	ACRN_HVLOG,
	ACRN_SEP,
	ACRN_SOCWATCH,
	/* The sbuf with above ids are created each pcpu */
	ACRN_SBUF_PER_PCPU_ID_MAX,
	ACRN_ASYNCIO = 64,
};

/* Make sure sizeof(struct shared_buf) == SBUF_HEAD_SIZE */
struct shared_buf {
	uint64_t magic;
	uint32_t ele_num;	/* number of elements */
	uint32_t ele_size;	/* sizeof of elements */
	uint32_t head;		/* offset from base, to read */
	uint32_t tail;		/* offset from base, to write */
	uint32_t flags;
	uint32_t reserved;
	uint32_t overrun_cnt;	/* count of overrun */
	uint32_t size;		/* ele_num * ele_size */
	uint32_t padding[6];
};

/**
 * @}
 */
#endif /* ACRN_COMMON_H */
