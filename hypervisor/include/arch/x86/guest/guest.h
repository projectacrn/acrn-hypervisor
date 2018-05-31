/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GUEST_H
#define GUEST_H

/* Defines for VM Launch and Resume */
#define VM_RESUME               0
#define VM_LAUNCH               1

#define ACRN_DBG_PTIRQ		6
#define ACRN_DBG_IRQ		6

#ifndef ASSEMBLER

#define foreach_vcpu(idx, vm, vcpu)				\
	for (idx = 0, vcpu = vm->hw.vcpu_array[idx];		\
		(idx < vm->hw.num_vcpus) & (vcpu != NULL);	\
		idx++, vcpu = vm->hw.vcpu_array[idx])


/* the index is matched with emulated msrs array*/
enum {
	IDX_TSC_DEADLINE,
	IDX_BIOS_UPDT_TRIG,
	IDX_BIOS_SIGN_ID,
	IDX_TSC,

	IDX_MAX_MSR
};

struct vhm_request;

int acrn_insert_request_wait(struct vcpu *vcpu, struct vhm_request *req);
int get_req_info(char *str, int str_max);

/*
 * VCPU related APIs
 */
#define ACRN_REQUEST_EXCP 0
#define ACRN_REQUEST_EVENT 1
#define ACRN_REQUEST_EXTINT 2
#define ACRN_REQUEST_NMI 3
#define	ACRN_REQUEST_TMR_UPDATE 4
#define	ACRN_REQUEST_EPT_FLUSH 5
#define	ACRN_REQUEST_TRP_FAULT 6
#define ACRN_REQUEST_VPID_FLUSH 7 /* flush vpid tlb */

#define E820_MAX_ENTRIES    32

struct e820_mem_params {
	uint64_t mem_bottom;
	uint64_t mem_top;
	uint64_t total_mem_size;
	uint64_t max_ram_blk_base; /* used for the start address of UOS */
	uint64_t max_ram_blk_size;
};

int prepare_vm0_memmap_and_e820(struct vm *vm);

/* Definition for a mem map lookup */
struct vm_lu_mem_map {
	struct list_head list;                 /* EPT mem map lookup list*/
	void *hpa;	/* Host physical start address of the map*/
	void *gpa;	/* Guest physical start address of the map */
	uint64_t size;	/* Size of map */
};

enum vm_cpu_mode {
	CPU_MODE_REAL,
	CPU_MODE_PROTECTED,
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

/* Use # of paging level to identify paging mode */
enum vm_paging_mode {
	PAGING_MODE_0_LEVEL = 0,	/* Flat */
	PAGING_MODE_2_LEVEL = 2,	/* 32bit paging, 2-level */
	PAGING_MODE_3_LEVEL = 3,	/* PAE paging, 3-level */
	PAGING_MODE_4_LEVEL = 4,	/* 64bit paging, 4-level */
	PAGING_MODE_NUM,
};

/*
 * VM related APIs
 */
bool is_vm0(struct vm *vm);
bool vm_lapic_disabled(struct vm *vm);
uint64_t vcpumask2pcpumask(struct vm *vm, uint64_t vdmask);

int gva2gpa(struct vcpu *vcpu, uint64_t gva, uint64_t *gpa, uint32_t *err_code);
int vm_gva2gpa(struct vcpu *vcpu, uint64_t gla, uint64_t *gpa,
	uint32_t *err_code);

struct vcpu *get_primary_vcpu(struct vm *vm);
struct vcpu *vcpu_from_vid(struct vm *vm, int vcpu_id);
struct vcpu *vcpu_from_pid(struct vm *vm, int pcpu_id);

enum vm_paging_mode get_vcpu_paging_mode(struct vcpu *vcpu);

void init_e820(void);
void obtain_e820_mem_info(void);
extern uint32_t e820_entries;
extern struct e820_entry e820[E820_MAX_ENTRIES];
extern uint32_t boot_regs[];
extern struct e820_mem_params e820_mem;

int rdmsr_vmexit_handler(struct vcpu *vcpu);
int wrmsr_vmexit_handler(struct vcpu *vcpu);
void init_msr_emulation(struct vcpu *vcpu);

extern const char vm_exit[];
int vmx_vmrun(struct run_context *context, int ops, int ibrs);

int load_guest(struct vm *vm, struct vcpu *vcpu);
int general_sw_loader(struct vm *vm, struct vcpu *vcpu);

typedef int (*vm_sw_loader_t)(struct vm *, struct vcpu *);
extern vm_sw_loader_t vm_sw_loader;

int copy_from_vm(struct vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);
int copy_to_vm(struct vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);

uint32_t create_guest_init_gdt(struct vm *vm, uint32_t *limit);
#endif	/* !ASSEMBLER */

#endif /* GUEST_H*/
