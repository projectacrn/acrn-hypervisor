/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_
#include <bsp_extern.h>
#include <vpci.h>

#ifdef CONFIG_PARTITION_MODE
#include <mptable.h>
#endif
enum vm_privilege_level {
	VM_PRIVILEGE_LEVEL_HIGH = 0,
	VM_PRIVILEGE_LEVEL_MEDIUM,
	VM_PRIVILEGE_LEVEL_LOW
};

#define INVALID_VM_ID 0xffffU

struct vm_hw_info {
	/* vcpu array of this VM */
	struct vcpu vcpu_array[CONFIG_MAX_VCPUS_PER_VM];
	uint16_t created_vcpus;	/* Number of created vcpus */
	uint64_t gpa_lowtop;    /* top lowmem gpa of this VM */
} __aligned(CPU_PAGE_SIZE);

struct sw_linux {
	void *ramdisk_src_addr;		/* HVA */
	void *ramdisk_load_addr;	/* GPA */
	uint32_t ramdisk_size;
	void *bootargs_src_addr;	/* HVA */
	void *bootargs_load_addr;	/* GPA */
	uint32_t bootargs_size;
	void *dtb_src_addr;		/* HVA */
	void *dtb_load_addr;		/* GPA */
	uint32_t dtb_size;
};

struct sw_kernel_info {
	void *kernel_src_addr;		/* HVA */
	void *kernel_load_addr;		/* GPA */
	void *kernel_entry_addr;	/* GPA */
	uint32_t kernel_size;
};

struct vm_sw_info {
	int kernel_type;	/* Guest kernel type */
	/* Kernel information (common for all guest types) */
	struct sw_kernel_info kernel_info;
	/* Additional information specific to Linux guests */
	struct sw_linux linux_info;
	/* HVA to IO shared page */
	void *io_shared_page;
};

struct vm_pm_info {
	uint8_t			px_cnt;		/* count of all Px states */
	struct cpu_px_data	px_data[MAX_PSTATE];
	uint8_t			cx_cnt;		/* count of all Cx entries */
	struct cpu_cx_data	cx_data[MAX_CSTATE];
	struct pm_s_state_data	*sx_state_data;	/* data for S3/S5 implementation */
};

/* VM guest types */
#define VM_LINUX_GUEST      0x02
#define VM_MONO_GUEST       0x01

enum vpic_wire_mode {
	VPIC_WIRE_INTR = 0,
	VPIC_WIRE_LAPIC,
	VPIC_WIRE_IOAPIC,
	VPIC_WIRE_NULL
};

/* Enumerated type for VM states */
enum vm_state {
	VM_CREATED = 0,	/* VM created / awaiting start (boot) */
	VM_STARTED,	/* VM started (booted) */
	VM_PAUSED,	/* VM paused */
	VM_STATE_UNKNOWN
};

struct vm_arch {
	/* I/O bitmaps A and B for this VM, MUST be 4-Kbyte aligned */
	uint8_t io_bitmap[CPU_PAGE_SIZE*2];
	/* MSR bitmap region for this VM, MUST be 4-Kbyte aligned */
	uint8_t msr_bitmap[CPU_PAGE_SIZE];

	uint64_t guest_init_pml4;/* Guest init pml4 */
	/* EPT hierarchy for Normal World */
	void *nworld_eptp;
	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	void *sworld_eptp;
	void *tmp_pg_array;	/* Page array for tmp guest paging struct */
	struct acrn_vioapic vioapic;	/* Virtual IOAPIC base address */
	struct acrn_vpic vpic;      /* Virtual PIC */
	/**
	 * A link to the IO handler of this VM.
	 * We only register io handle to this link
	 * when create VM on sequences and ungister it when
	 * destory VM. So there no need lock to prevent preempt.
	 * Besides, there only a few io handlers now, we don't
	 * need binary search temporary.
	 */
	struct vm_io_handler *io_handler;

	/* reference to virtual platform to come here (as needed) */
} __aligned(CPU_PAGE_SIZE);


#define CPUID_CHECK_SUBLEAF	(1U << 0U)
#define MAX_VM_VCPUID_ENTRIES	64U
struct vcpuid_entry {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t leaf;
	uint32_t subleaf;
	uint32_t flags;
	uint32_t padding;
};

struct vm {
	struct vm_arch arch_vm; /* Reference to this VM's arch information */
	struct vm_hw_info hw;	/* Reference to this VM's HW information */
	struct vm_sw_info sw;	/* Reference to SW associated with this VM */
	struct vm_pm_info pm;	/* Reference to this VM's arch information */
	uint16_t vm_id;		    /* Virtual machine identifier */
	enum vm_state state;	/* VM state */
	struct acrn_vuart vuart;		/* Virtual UART */
	enum vpic_wire_mode wire_mode;
	struct iommu_domain *iommu;	/* iommu domain of this VM */
	spinlock_t spinlock;	/* Spin-lock used to protect VM modifications */

	struct list_head mmio_list; /* list for mmio. This list is not updated
				     * when vm is active. So no lock needed
				     */

	unsigned char GUID[16];
	struct secure_world_control sworld_control;

	/* Secure World's snapshot
	 * Currently, Secure World is only running on vcpu[0],
	 * so the snapshot only stores the vcpu0's run_context
	 * of secure world.
	 */
	struct cpu_context sworld_snapshot;

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];
	struct vpci vpci;
#ifdef CONFIG_PARTITION_MODE
	struct vm_description	*vm_desc;
	uint8_t vrtc_offset;
#endif

	spinlock_t softirq_dev_lock;
	struct list_head softirq_dev_entry_list;
	uint64_t intr_inject_delay_delta; /* delay of intr injection */
} __aligned(CPU_PAGE_SIZE);

#ifdef CONFIG_PARTITION_MODE
struct vpci_vdev_array {
	int num_pci_vdev;
	struct pci_vdev vpci_vdev_list[];
};
#endif

struct vm_description {
	/* The physical CPU IDs associated with this VM - The first CPU listed
	 * will be the VM's BSP
	 */
	uint16_t               *vm_pcpu_ids;
	unsigned char          GUID[16]; /* GUID of the vm will be created */
	uint16_t               vm_hw_num_cores;   /* Number of virtual cores */
	/* Whether secure world is supported for current VM. */
	bool                   sworld_supported;
#ifdef CONFIG_PARTITION_MODE
	uint8_t			vm_id;
	struct mptable_info	*mptable;
	uint64_t		start_hpa;
	uint64_t		mem_size; /* UOS memory size in hex */
	bool			vm_vuart;
	const char		*bootargs;
	struct vpci_vdev_array  *vpci_vdev_array;
#endif
};

static inline bool is_vm0(const struct vm *vm)
{
	return (vm->vm_id) == 0U;
}

/*
 * @pre vcpu_id < CONFIG_MAX_VCPUS_PER_VM
 */
static inline struct vcpu *vcpu_from_vid(struct vm *vm, uint16_t vcpu_id)
{
	uint16_t i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->vcpu_id == vcpu_id) {
			break;
		}
	}
	return vcpu;
}

static inline struct vcpu *vcpu_from_pid(struct vm *vm, uint16_t pcpu_id)
{
	uint16_t i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->pcpu_id == pcpu_id) {
			return vcpu;
		}
	}

	return NULL;
}

static inline struct vcpu *get_primary_vcpu(struct vm *vm)
{
	uint16_t i;
	struct vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		if (is_vcpu_bsp(vcpu)) {
			return vcpu;
		}
	}

	return NULL;
}

static inline struct acrn_vuart*
vm_vuart(struct vm *vm)
{
	return &(vm->vuart);
}

static inline struct acrn_vpic *
vm_pic(struct vm *vm)
{
	return (struct acrn_vpic *)&(vm->arch_vm.vpic);
}

static inline struct acrn_vioapic *
vm_ioapic(struct vm *vm)
{
	return (struct acrn_vioapic *)&(vm->arch_vm.vioapic);
}

int shutdown_vm(struct vm *vm);
void pause_vm(struct vm *vm);
void resume_vm(struct vm *vm);
void resume_vm_from_s3(struct vm *vm, uint32_t wakeup_vec);
int start_vm(struct vm *vm);
int reset_vm(struct vm *vm);
int create_vm(struct vm_description *vm_desc, struct vm **rtn_vm);
int prepare_vm(uint16_t pcpu_id);

#ifdef CONFIG_PARTITION_MODE
const struct vm_description_array *get_vm_desc_base(void);
#endif

struct vm *get_vm_from_vmid(uint16_t vm_id);

#ifdef CONFIG_PARTITION_MODE
struct vm_description_array {
	int                     num_vm_desc;
	struct vm_description   vm_desc_array[];
};

struct pcpu_vm_desc_mapping {
	struct vm_description *vm_desc_ptr;
	bool is_bsp;
};
extern const struct pcpu_vm_desc_mapping pcpu_vm_desc_map[];

void vrtc_init(struct vm *vm);
#endif
#endif /* VM_H_ */
