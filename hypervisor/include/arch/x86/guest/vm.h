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

#include <bits.h>
#include <spinlock.h>
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
#ifdef CONFIG_HYPERV_ENABLED
#include <hyperv.h>
#endif

enum reset_mode {
	POWER_ON_RESET,		/* reset by hardware Power-on */
	COLD_RESET,		/* hardware cold reset */
	WARM_RESET,		/* behavior slightly differ from cold reset, that some MSRs might be retained. */
	INIT_RESET,		/* reset by INIT */
	SOFTWARE_RESET,		/* reset by software disable<->enable */
};

struct vm_hw_info {
	/* vcpu array of this VM */
	struct acrn_vcpu vcpu_array[MAX_VCPUS_PER_VM];
	uint16_t created_vcpus;	/* Number of created vcpus */
	uint64_t cpu_affinity;	/* Actual pCPUs this VM runs on. The set bits represent the pCPU IDs */
} __aligned(PAGE_SIZE);

struct sw_module_info {
	/* sw modules like ramdisk, bootargs, firmware, etc. */
	void *src_addr;			/* HVA */
	void *load_addr;		/* GPA */
	uint32_t size;
};

struct sw_kernel_info {
	void *kernel_src_addr;		/* HVA */
	void *kernel_load_addr;		/* GPA */
	void *kernel_entry_addr;	/* GPA */
	uint32_t kernel_size;
};

struct vm_sw_info {
	enum os_kernel_type kernel_type;	/* Guest kernel type */
	/* Kernel information (common for all guest types) */
	struct sw_kernel_info kernel_info;
	struct sw_module_info bootargs_info;
	struct sw_module_info ramdisk_info;
	/* HVA to IO shared page */
	void *io_shared_page;
	/* If enable IO completion polling mode */
	bool is_polling_ioreq;
};

struct vm_pm_info {
	uint8_t			px_cnt;		/* count of all Px states */
	struct cpu_px_data	px_data[MAX_PSTATE];
	uint8_t			cx_cnt;		/* count of all Cx entries */
	struct cpu_cx_data	cx_data[MAX_CSTATE];
	struct pm_s_state_data	*sx_state_data;	/* data for S3/S5 implementation */
};

/* Enumerated type for VM states */
enum vm_state {
	VM_POWERED_OFF = 0,   /* MUST set 0 because vm_state's initialization depends on clear BSS section */
	VM_CREATED,	/* VM created / awaiting start (boot) */
	VM_RUNNING,	/* VM running */
	VM_READY_TO_POWEROFF,     /* RTVM only, it is trying to poweroff by itself */
	VM_PAUSED,	/* VM paused */
};

enum vm_vlapic_mode {
	VM_VLAPIC_DISABLED = 0U,
	VM_VLAPIC_XAPIC,
	VM_VLAPIC_X2APIC,
	VM_VLAPIC_TRANSITION
};

struct vm_arch {
	/* I/O bitmaps A and B for this VM, MUST be 4-Kbyte aligned */
	uint8_t io_bitmap[PAGE_SIZE*2];

	/* EPT hierarchy for Normal World */
	void *nworld_eptp;
	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	void *sworld_eptp;
	struct memory_ops ept_mem_ops;

	struct acrn_vioapics vioapics;	/* Virtual IOAPIC/s */
	struct acrn_vpic vpic;      /* Virtual PIC */
#ifdef CONFIG_HYPERV_ENABLED
	struct acrn_hyperv hyperv;
#endif
	enum vm_vlapic_mode vlapic_mode; /* Represents vLAPIC mode across vCPUs*/

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
	struct acrn_vuart vuart[MAX_VUART_NUM_PER_VM];		/* Virtual UART */
	enum vpic_wire_mode wire_mode;
	struct iommu_domain *iommu;	/* iommu domain of this VM */
	/* vm_state_lock used to protect vm/vcpu state transition,
	 * the initialization depends on the clear BSS section
	 */
	spinlock_t vm_state_lock;
	spinlock_t vlapic_mode_lock;	/* Spin-lock used to protect vlapic_mode modifications for a VM */
	spinlock_t ept_lock;	/* Spin-lock used to protect ept add/modify/remove for a VM */
	spinlock_t emul_mmio_lock;	/* Used to protect emulation mmio_node concurrent access for a VM */
	uint16_t nr_emul_mmio_regions;	/* the emulated mmio_region number */
	struct mem_io_node emul_mmio[CONFIG_MAX_EMULATED_MMIO_REGIONS];

	struct vm_io_handler_desc emul_pio[EMUL_PIO_IDX_MAX];

	uint8_t uuid[16];
	struct secure_world_control sworld_control;

	/* Secure World's snapshot
	 * Currently, Secure World is only running on vcpu[0],
	 * so the snapshot only stores the vcpu0's run_context
	 * of secure world.
	 */
	struct guest_cpu_context sworld_snapshot;

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];
	struct acrn_vpci vpci;
	uint8_t vrtc_offset;

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
 * @pre vcpu_id < MAX_VCPUS_PER_VM
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
		if (pcpuid_from_vcpu(vcpu) == pcpu_id) {
			target_vcpu = vcpu;
			break;
		}
	}

	return target_vcpu;
}

/* Convert relative vm id to absolute vm id */
static inline uint16_t rel_vmid_2_vmid(uint16_t sos_vmid, uint16_t rel_vmid) {
	return (sos_vmid + rel_vmid);
}

/* Convert absolute vm id to relative vm id */
static inline uint16_t vmid_2_rel_vmid(uint16_t sos_vmid, uint16_t vmid) {
	return (vmid - sos_vmid);
}

void make_shutdown_vm_request(uint16_t pcpu_id);
bool need_shutdown_vm(uint16_t pcpu_id);
int32_t shutdown_vm(struct acrn_vm *vm);
void pause_vm(struct acrn_vm *vm);
void resume_vm_from_s3(struct acrn_vm *vm, uint32_t wakeup_vec);
void start_vm(struct acrn_vm *vm);
int32_t reset_vm(struct acrn_vm *vm);
int32_t create_vm(uint16_t vm_id, uint64_t pcpu_bitmap, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm);
void prepare_vm(uint16_t vm_id, struct acrn_vm_config *vm_config);
void launch_vms(uint16_t pcpu_id);
bool is_poweroff_vm(const struct acrn_vm *vm);
bool is_created_vm(const struct acrn_vm *vm);
bool is_paused_vm(const struct acrn_vm *vm);
bool is_sos_vm(const struct acrn_vm *vm);
bool is_postlaunched_vm(const struct acrn_vm *vm);
bool is_valid_postlaunched_vmid(uint16_t vm_id);
bool is_prelaunched_vm(const struct acrn_vm *vm);
uint16_t get_vmid_by_uuid(const uint8_t *uuid);
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id);
struct acrn_vm *get_sos_vm(void);

void create_sos_vm_e820(struct acrn_vm *vm);
void create_prelaunched_vm_e820(struct acrn_vm *vm);

int32_t direct_boot_sw_loader(struct acrn_vm *vm);

typedef int32_t (*vm_sw_loader_t)(struct acrn_vm *vm);
extern vm_sw_loader_t vm_sw_loader;

void vrtc_init(struct acrn_vm *vm);

bool is_lapic_pt_configured(const struct acrn_vm *vm);
bool is_rt_vm(const struct acrn_vm *vm);
bool is_pi_capable(const struct acrn_vm *vm);
bool has_rt_vm(void);
struct acrn_vm *get_highest_severity_vm(bool runtime);
bool vm_hide_mtrr(const struct acrn_vm *vm);
void update_vm_vlapic_state(struct acrn_vm *vm);
enum vm_vlapic_mode check_vm_vlapic_mode(const struct acrn_vm *vm);
/*
 * @pre vm != NULL
 */
void get_vm_lock(struct acrn_vm *vm);

/*
 * @pre vm != NULL
 */
void put_vm_lock(struct acrn_vm *vm);
#endif /* !ASSEMBLER */

#endif /* VM_H_ */
