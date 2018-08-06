/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM_H_
#define VM_H_
#include <bsp_extern.h>

#ifdef CONFIG_PARTITION_MODE
#include <mptable.h>
#endif
enum vm_privilege_level {
	VM_PRIVILEGE_LEVEL_HIGH = 0,
	VM_PRIVILEGE_LEVEL_MEDIUM,
	VM_PRIVILEGE_LEVEL_LOW
};

#define	MAX_VM_NAME_LEN		16
#define INVALID_VM_ID 0xffffU

struct vm_hw_info {
	uint16_t num_vcpus;	/* Number of total virtual cores */
	uint16_t exp_num_vcpus;	/* Number of real expected virtual cores */
	uint16_t created_vcpus;	/* Number of created vcpus */
	struct vcpu **vcpu_array;	/* vcpu array of this VM */
	uint64_t gpa_lowtop;    /* top lowmem gpa of this VM */
};

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
	uint64_t guest_init_pml4;/* Guest init pml4 */
	/* EPT hierarchy for Normal World */
	void *nworld_eptp;
	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	void *sworld_eptp;
	void *m2p;		/* machine address to guest physical address */
	void *tmp_pg_array;	/* Page array for tmp guest paging struct */
	void *iobitmap[2];/* IO bitmap page array base address for this VM */
	void *msr_bitmap;	/* MSR bitmap page base address for this VM */
	void *virt_ioapic;	/* Virtual IOAPIC base address */
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
};

#define CPUID_CHECK_SUBLEAF	(1U << 0)
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

struct acrn_vpic;
struct vm {
	uint16_t vm_id;		    /* Virtual machine identifier */
	struct vm_hw_info hw;	/* Reference to this VM's HW information */
	struct vm_sw_info sw;	/* Reference to SW associated with this VM */
	struct vm_pm_info pm;	/* Reference to this VM's arch information */
	struct vm_arch arch_vm;	/* Reference to this VM's arch information */
	enum vm_state state;	/* VM state */
	void *vuart;		/* Virtual UART */
	struct acrn_vpic *vpic;      /* Virtual PIC */
	enum vpic_wire_mode wire_mode;
	struct iommu_domain *iommu;	/* iommu domain of this VM */
	struct list_head list; /* list of VM */
	spinlock_t spinlock;	/* Spin-lock used to protect VM modifications */

	struct list_head mmio_list; /* list for mmio. This list is not updated
				     * when vm is active. So no lock needed
				     */

	struct _vm_shared_memory *shared_memory_area;

	struct {
		struct _vm_virtual_device_node *head;
		struct _vm_virtual_device_node *tail;
	} virtual_device_list;

	unsigned char GUID[16];
	struct secure_world_control sworld_control;

	uint32_t vcpuid_entry_nr, vcpuid_level, vcpuid_xlevel;
	struct vcpuid_entry vcpuid_entries[MAX_VM_VCPUID_ENTRIES];
#ifdef CONFIG_PARTITION_MODE
	struct vm_description	*vm_desc;
#endif
};

struct vm_description {
	/* The physical CPU IDs associated with this VM - The first CPU listed
	 * will be the VM's BSP
	 */
	uint16_t               *vm_pcpu_ids;
	unsigned char          GUID[16]; /* GUID of the vm will be created */
	uint16_t               vm_hw_num_cores;   /* Number of virtual cores */
	/* Whether secure world is enabled for current VM. */
	bool                   sworld_enabled;
#ifdef CONFIG_PARTITION_MODE
	struct mptable_info	*mptable;
#endif
};

int shutdown_vm(struct vm *vm);
void pause_vm(struct vm *vm);
void resume_vm(struct vm *vm);
void resume_vm_from_s3(struct vm *vm, uint32_t wakeup_vec);
int start_vm(struct vm *vm);
int reset_vm(struct vm *vm);
int create_vm(struct vm_description *vm_desc, struct vm **rtn_vm);
int prepare_vm0(void);
#ifdef CONFIG_VM0_DESC
void vm_fixup(struct vm *vm);
#endif

struct vm *get_vm_from_vmid(uint16_t vm_id);

extern struct list_head vm_list;
extern spinlock_t vm_list_lock;
extern bool x2apic_enabled;

#endif /* VM_H_ */
