/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_

/* Defines for VM Launch and Resume */
#define VM_RESUME		0
#define VM_LAUNCH		1

#ifndef ASSEMBLER

#include <list.h>
#include <types.h>
#include <bits.h>
#include <mmu.h>
#include <dm/vuart.h>
#include <dm/io_req.h>
#include <dm/vpci.h>
#include <dm/vrtc.h>
#include <spinlock.h>
#include <vm_config.h>
#include <asm/guest/vm.h>

#include <vcpu.h>

#define	NEED_SHUTDOWN_VM	(2U)

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
	void *kernel_entry_addr;	/* GPA */
	uint32_t kernel_size;
};

struct vm_sw_info {
	enum os_kernel_type kernel_type;	/* Guest kernel type */
	/* Kernel information (common for all guest types) */
	struct sw_kernel_info kernel_info;
	struct sw_module_info bootargs_info;
	struct sw_module_info ramdisk_info;
	struct sw_module_info acpi_info;
	struct sw_module_info fdt_info;
	/* HVA to IO shared page */
	void *io_shared_page;
	void *asyncio_sbuf;
	void *vm_event_sbuf;
	/* If enable IO completion polling mode */
	bool is_polling_ioreq;
};

/* Enumerated type for VM states */
enum vm_state {
	VM_POWERED_OFF = 0,   /* MUST set 0 because vm_state's initialization depends on clear BSS section */
	VM_CREATED,	/* VM created / awaiting start (boot) */
	VM_RUNNING,	/* VM running */
	VM_READY_TO_POWEROFF,     /* RTVM only, it is trying to poweroff by itself */
	VM_PAUSED,	/* VM paused */
};

struct acrn_vm {
	struct vm_arch arch_vm; /* Reference to this VM's arch information */
	struct vm_hw_info hw;	/* Reference to this VM's HW information */
	struct vm_sw_info sw;	/* Reference to SW associated with this VM */
	uint16_t vm_id;		    /* Virtual machine identifier */
	enum vm_state state;	/* VM state */
	struct acrn_vuart vuart[MAX_VUART_NUM_PER_VM];		/* Virtual UART */
	struct asyncio_desc	aio_desc[ACRN_ASYNCIO_MAX];
	struct list_head aiodesc_queue;
	spinlock_t asyncio_lock; /* Spin-lock used to protect asyncio add/remove for a VM */
	spinlock_t vm_event_lock;
	spinlock_t vm_state_lock;
	struct iommu_domain *iommu;	/* iommu domain of this VM */
	char name[MAX_VM_NAME_LEN];
	struct acrn_vpci vpci;
	struct acrn_vrtc vrtc;

	spinlock_t emul_mmio_lock;	/* Used to protect emulation mmio_node concurrent access for a VM */
	uint16_t nr_emul_mmio_regions;	/* the emulated mmio_region number */
	struct mem_io_node emul_mmio[CONFIG_MAX_EMULATED_MMIO_REGIONS];
	struct vm_io_handler_desc emul_pio[EMUL_PIO_IDX_MAX];

	/* Pointer to root stage2 pagetable */
	void *root_stg2ptp;
	struct pgtable stg2_pgtable;
	spinlock_t stg2pt_lock;	/* Spin-lock used to protect stg2pt to add/modify/remove for a VM */
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
		bitmap_set_non_atomic(vcpu->vcpu_id, &dmask);
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

struct acrn_vm *get_vm_from_vmid(__unused uint16_t vm_id);

bool is_paused_vm(__unused const struct acrn_vm *vm);

bool is_poweroff_vm(__unused const struct acrn_vm *vm);

bool is_ready_for_system_shutdown(void);

void arch_trigger_level_intr(__unused struct acrn_vm *vm,
		__unused uint32_t irq, __unused bool assert);

/* Convert relative vm id to absolute vm id */
static inline uint16_t rel_vmid_2_vmid(uint16_t service_vmid, uint16_t rel_vmid) {
	return (service_vmid + rel_vmid);
}

/* Convert absolute vm id to relative vm id */
static inline uint16_t vmid_2_rel_vmid(uint16_t service_vmid, uint16_t vmid) {
	return (vmid - service_vmid);
}

static inline bool is_severity_pass(uint16_t target_vmid)
{
	return SEVERITY_SERVICE_VM >= get_vm_severity(target_vmid);
}

void shutdown_vm_from_idle(uint16_t pcpu_id);
void make_shutdown_vm_request(uint16_t pcpu_id);
bool need_shutdown_vm(uint16_t pcpu_id);
void poweroff_if_rt_vm(struct acrn_vm *vm);
bool is_poweroff_vm(const struct acrn_vm *vm);
bool is_created_vm(const struct acrn_vm *vm);
bool is_paused_vm(const struct acrn_vm *vm);
bool is_service_vm(const struct acrn_vm *vm);
bool is_postlaunched_vm(const struct acrn_vm *vm);
bool is_prelaunched_vm(const struct acrn_vm *vm);
uint16_t get_vmid_by_name(const char *name);
struct acrn_vm *get_vm_from_vmid(uint16_t vm_id);
struct acrn_vm *get_service_vm(void);
bool is_rt_vm(const struct acrn_vm *vm);
bool is_stateful_vm(const struct acrn_vm *vm);
bool is_static_configured_vm(const struct acrn_vm *vm);
uint16_t get_unused_vmid(void);
bool has_rt_vm(void);
struct acrn_vm *get_highest_severity_vm(bool runtime);
int32_t prepare_os_image(struct acrn_vm *vm);


/*
 * @pre vm != NULL
 */
void get_vm_lock(struct acrn_vm *vm);

/*
 * @pre vm != NULL
 */
void put_vm_lock(struct acrn_vm *vm);

int32_t arch_init_vm(struct acrn_vm *vm, struct acrn_vm_config *vm_config);
int32_t arch_deinit_vm(struct acrn_vm *vm);
void arch_vm_prepare_bsp(struct acrn_vcpu *bsp);
int32_t arch_reset_vm(struct acrn_vm *vm);

int32_t create_vm(uint16_t vm_id, uint64_t pcpu_bitmap, struct acrn_vm_config *vm_config, struct acrn_vm **rtn_vm);
void launch_vms(uint16_t pcpu_id);
void start_vm(struct acrn_vm *vm);
void pause_vm(struct acrn_vm *vm);
int32_t destroy_vm(struct acrn_vm *vm);
int32_t reset_vm(struct acrn_vm *vm);

#endif /* !ASSEMBLER */

#endif /* VM_H_ */
