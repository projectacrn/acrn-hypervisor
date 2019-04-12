/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_

/* Defines for VM Launch and Resume */
#define VM_RESUME		0
#define VM_LAUNCH		1

#ifndef ASSEMBLER

#include <types.h>
#include <bits.h>
#include <spinlock.h>
#include <acrn_common.h>
#include <vcpu.h>
#include <vioapic.h>
#include <vpic.h>
#include <vmx_io.h>
#include <vuart.h>
#include <trusty.h>
#include <vcpuid.h>
#include <vpci.h>
#include <cpu_caps.h>
#include <e820.h>
#include <vm_config.h>

#ifdef CONFIG_PARTITION_MODE
#include <mptable.h>
#endif

#define INVALID_VM_ID 0xffffU

struct vm_hw_info {
	/* vcpu array of this VM */
	struct acrn_vcpu vcpu_array[CONFIG_MAX_VCPUS_PER_VM];
	uint16_t created_vcpus;	/* Number of created vcpus */
} __aligned(PAGE_SIZE);

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
	int32_t kernel_type;	/* Guest kernel type */
	/* Kernel information (common for all guest types) */
	struct sw_kernel_info kernel_info;
	/* Additional information specific to Linux guests */
	struct sw_linux linux_info;
	/* HVA to IO shared page */
	void *io_shared_page;
	/* If enable IO completion polling mode */
	bool is_completion_polling;
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
/* Enumerated type for VM states */
enum vm_state {
	VM_STATE_INVALID = 0,
	VM_CREATED,	/* VM created / awaiting start (boot) */
	VM_STARTED,	/* VM started (booted) */
	VM_POWERING_OFF,     /* RTVM only, it is trying to poweroff by itself */
	VM_PAUSED,	/* VM paused */
};

struct vm_arch {
	/* I/O bitmaps A and B for this VM, MUST be 4-Kbyte aligned */
	uint8_t io_bitmap[PAGE_SIZE*2];
	/* MSR bitmap region for this VM, MUST be 4-Kbyte aligned */
	uint8_t msr_bitmap[PAGE_SIZE];

	uint64_t guest_init_pml4;/* Guest init pml4 */
	/* EPT hierarchy for Normal World */
	void *nworld_eptp;
	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	void *sworld_eptp;
	struct memory_ops ept_mem_ops;

	void *tmp_pg_array;	/* Page array for tmp guest paging struct */
	struct acrn_vioapic vioapic;	/* Virtual IOAPIC base address */
	struct acrn_vpic vpic;      /* Virtual PIC */

	/* reference to virtual platform to come here (as needed) */
} __aligned(PAGE_SIZE);

struct acrn_vm {
	struct vm_arch arch_vm; /* Reference to this VM's arch information */
	struct vm_hw_info hw;	/* Reference to this VM's HW information */
	struct vm_sw_info sw;	/* Reference to SW associated with this VM */
	struct vm_pm_info pm;	/* Reference to this VM's arch information */
	uint32_t e820_entry_num;
	struct e820_entry *e820_entries;
	uint16_t vm_id;		    /* Virtual machine identifier */
	enum vm_state state;	/* VM state */
	struct acrn_vuart vuart;		/* Virtual UART */
	enum vpic_wire_mode wire_mode;
	struct iommu_domain *iommu;	/* iommu domain of this VM */
	spinlock_t spinlock;	/* Spin-lock used to protect VM modifications */

	uint16_t emul_mmio_regions; /* Number of emulated mmio regions */
	struct mem_io_node emul_mmio[CONFIG_MAX_EMULATED_MMIO_REGIONS];
	hv_mem_io_handler_t default_read_write;

	struct vm_io_handler_desc emul_pio[EMUL_PIO_IDX_MAX];
	io_read_fn_t default_io_read;
	io_write_fn_t default_io_write;

	uint8_t uuid[16];
	struct secure_world_control sworld_control;

	/* Secure World's snapshot
	 * Currently, Secure World is only running on vcpu[0],
	 * so the snapshot only stores the vcpu0's run_context
	 * of secure world.
	 */
	struct cpu_context sworld_snapshot;

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];
	struct acrn_vpci vpci;

#ifdef CONFIG_PARTITION_MODE
	struct mptable_info mptable;
#endif
	uint8_t vrtc_offset;

	spinlock_t softirq_dev_lock;
	struct list_head softirq_dev_entry_list;
	uint64_t intr_inject_delay_delta; /* delay of intr injection */
} __aligned(PAGE_SIZE);

/*
 * @pre vlapic != NULL
 */
static inline uint64_t vm_active_cpus(const struct acrn_vm *vm)
{
	uint64_t dmask = 0UL;
	uint16_t i;
	const struct acrn_vcpu *vcpu;

	foreach_vcpu(i, vm, vcpu) {
		bitmap_set_nolock(vcpu->vcpu_id, &dmask);
	}

	return dmask;
}

/*
 * @pre vcpu_id < CONFIG_MAX_VCPUS_PER_VM
 * @pre &(vm->hw.vcpu_array[vcpu_id])->state != VCPU_OFFLINE
 */
static inline struct acrn_vcpu *vcpu_from_vid(struct acrn_vm *vm, uint16_t vcpu_id)
{
	return &(vm->hw.vcpu_array[vcpu_id]);
}

static inline struct acrn_vcpu *vcpu_from_pid(struct acrn_vm *vm, uint16_t pcpu_id)
{
	uint16_t i;
	struct acrn_vcpu *vcpu, *target_vcpu = NULL;

	foreach_vcpu(i, vm, vcpu) {
		if (vcpu->pcpu_id == pcpu_id) {
			target_vcpu = vcpu;
			break;
		}
	}

	return target_vcpu;
}

int32_t shutdown_vm(struct acrn_vm *vm);
void pause_vm(struct acrn_vm *vm);
void resume_vm(struct acrn_vm *vm);
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec);
void start_vm(struct acrn_vm *vm);
int32_t reset_vm(struct acrn_vm *vm);
int32_t create_vm(uint16_t vm_id, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm);
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config);
void launch_vms(uint16_t pcpu_id);
bool is_valid_vm(const struct acrn_vm *vm);
bool is_sos_vm(const struct acrn_vm *vm);
bool is_prelaunched_vm(const struct acrn_vm *vm);
uint16_t find_free_vm_id(void);
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id);
struct acrn_vm *get_sos_vm(void);

int32_t general_sw_loader(struct acrn_vm *vm);

typedef int32_t (*vm_sw_loader_t)(struct acrn_vm *vm);
extern vm_sw_loader_t vm_sw_loader;

#ifdef CONFIG_PARTITION_MODE
uint16_t get_vm_pcpu_nums(const struct acrn_vm_config *vm_config);
#endif
void vrtc_init(struct acrn_vm *vm);

bool is_lapic_pt(const struct acrn_vm *vm);
bool is_rt_vm(const struct acrn_vm *vm);
bool vm_hide_mtrr(const struct acrn_vm *vm);

#endif /* !ASSEMBLER */

#endif /* VM_H_ */
