/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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

#ifndef VM_H_
#define VM_H_

enum vm_privilege_level {
	VM_PRIVILEGE_LEVEL_HIGH = 0,
	VM_PRIVILEGE_LEVEL_MEDIUM,
	VM_PRIVILEGE_LEVEL_LOW
};

#define	MAX_VM_NAME_LEN		16
struct vm_attr {
	char name[MAX_VM_NAME_LEN];	/* Virtual machine name string */
	int id;		/* Virtual machine identifier */
	int boot_idx;	/* Index indicating the boot sequence for this VM */
};

struct vm_hw_info {
	int num_vcpus;	/* Number of total virtual cores */
	int exp_num_vcpus;	/* Number of real expected virtual cores */
	uint32_t created_vcpus;	/* Number of created vcpus */
	struct vcpu **vcpu_array;	/* vcpu array of this VM */
	uint64_t gpa_lowtop;    /* top lowmem gpa of this VM */
};

struct sw_linux {
	void *ramdisk_src_addr;
	void *ramdisk_load_addr;
	uint32_t ramdisk_size;
	void *bootargs_src_addr;
	void *bootargs_load_addr;
	uint32_t bootargs_size;
	void *dtb_src_addr;
	void *dtb_load_addr;
	uint32_t dtb_size;
};

struct sw_kernel_info {
	void *kernel_src_addr;
	void *kernel_load_addr;
	void *kernel_entry_addr;
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

/* Structure for VM state information */
struct vm_state_info {
	enum vm_state state;	/* State of the VM */
	unsigned int privilege;	/* Privilege level of the VM */
	unsigned int boot_count;/* Number of times the VM has booted */

};

struct vm_arch {
	uint64_t guest_init_pml4;/* Guest init pml4 */
	/* EPT hierarchy for Normal World */
	uint64_t nworld_eptp;
	/* EPT hierarchy for Secure World
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 */
	uint64_t sworld_eptp;
	uint64_t m2p;		/* machine address to guest physical address */
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

#define CPUID_CHECK_SUBLEAF	(1 << 0)
#define MAX_VM_VCPUID_ENTRIES	64
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

struct vpic;
struct vm {
	struct vm_attr attr;	/* Reference to this VM's attributes */
	struct vm_hw_info hw;	/* Reference to this VM's HW information */
	struct vm_sw_info sw;	/* Reference to SW associated with this VM */
	struct vm_pm_info pm;	/* Reference to this VM's arch information */
	struct vm_arch arch_vm;	/* Reference to this VM's arch information */
	struct vm_state_info state_info;/* State info of this VM */
	enum vm_state state;	/* VM state */
	struct vcpu *current_vcpu;	/* VCPU that caused vm exit */
	void *vuart;		/* Virtual UART */
	struct vpic *vpic;      /* Virtual PIC */
	uint32_t vpic_wire_mode;
	struct iommu_domain *iommu_domain;	/* iommu domain of this VM */
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
};

struct vm_description {
	/* Virtual machine identifier, assigned by the system */
	char                   *vm_attr_name;
	/* The logical CPU IDs associated with this VM - The first CPU listed
	 * will be the VM's BSP
	 */
	int                    *vm_hw_logical_core_ids;
	unsigned char          GUID[16]; /* GUID of the vm will be created */
	int                    vm_hw_num_cores;   /* Number of virtual cores */
	/* Indicates to APs that the BSP has created a VM for this
	 * description
	 */
	bool                   vm_created;
	/* Index indicating VM's privilege level */
	unsigned int           vm_state_info_privilege;
	/* Whether secure world is enabled for current VM. */
	bool                   sworld_enabled;
};

struct vm_description_array {
	int                     num_vm_desc;
	struct vm_description   vm_desc_array[];
};

int shutdown_vm(struct vm *vm);
int pause_vm(struct vm *vm);
int start_vm(struct vm *vm);
int create_vm(struct vm_description *vm_desc, struct vm **vm);
int prepare_vm0(void);
void vm_fixup(struct vm *vm);

struct vm *get_vm_from_vmid(int vm_id);
struct vm_description *get_vm_desc(int idx);

extern struct list_head vm_list;
extern spinlock_t vm_list_lock;
extern bool x2apic_enabled;

#endif /* VM_H_ */
