/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file guest.h
 *
 * @brief Data transferring between hypervisor and VM
 */
#ifndef GUEST_H
#define GUEST_H

/* Defines for VM Launch and Resume */
#define VM_RESUME               0
#define VM_LAUNCH               1

#define ACRN_DBG_PTIRQ		6U
#define ACRN_DBG_IRQ		6U

#ifndef ASSEMBLER

#include <mmu.h>

#define foreach_vcpu(idx, vm, vcpu)				\
	for ((idx) = 0U, vcpu = &(vm->hw.vcpu_array[(idx)]);	\
		(idx) < vm->hw.created_vcpus;			\
		(idx)++, vcpu = &(vm->hw.vcpu_array[(idx)])) \
		if (vcpu->state != VCPU_OFFLINE)

/*
 * VCPU related APIs
 */
#define ACRN_REQUEST_EXCP      0U
#define ACRN_REQUEST_EVENT    1U
#define ACRN_REQUEST_EXTINT   2U
#define ACRN_REQUEST_NMI        3U
#define ACRN_REQUEST_TMR_UPDATE  4U
#define ACRN_REQUEST_EPT_FLUSH      5U
#define ACRN_REQUEST_TRP_FAULT      6U
#define ACRN_REQUEST_VPID_FLUSH    7U /* flush vpid tlb */

#define E820_MAX_ENTRIES    32U

#define save_segment(seg, SEG_NAME)				\
{								\
	(seg).selector = exec_vmread16(SEG_NAME##_SEL);		\
	(seg).base = exec_vmread(SEG_NAME##_BASE);		\
	(seg).limit = exec_vmread32(SEG_NAME##_LIMIT);		\
	(seg).attr = exec_vmread32(SEG_NAME##_ATTR);		\
}

#define load_segment(seg, SEG_NAME)				\
{								\
	exec_vmwrite16(SEG_NAME##_SEL, (seg).selector);		\
	exec_vmwrite(SEG_NAME##_BASE, (seg).base);		\
	exec_vmwrite32(SEG_NAME##_LIMIT, (seg).limit);		\
	exec_vmwrite32(SEG_NAME##_ATTR, (seg).attr);		\
}

/* Define segments constants for guest */
#define REAL_MODE_BSP_INIT_CODE_SEL     (0xf000U)
#define REAL_MODE_DATA_SEG_AR           (0x0093U)
#define REAL_MODE_CODE_SEG_AR           (0x009fU)
#define PROTECTED_MODE_DATA_SEG_AR      (0xc093U)
#define PROTECTED_MODE_CODE_SEG_AR      (0xc09bU)
#define REAL_MODE_SEG_LIMIT             (0xffffU)
#define PROTECTED_MODE_SEG_LIMIT        (0xffffffffU)
#define DR7_INIT_VALUE                  (0x400UL)
#define LDTR_AR                         (0x0082U) /* LDT, type must be 2, refer to SDM Vol3 26.3.1.2 */
#define TR_AR                           (0x008bU) /* TSS (busy), refer to SDM Vol3 26.3.1.2 */

int prepare_vm0_memmap_and_e820(struct acrn_vm *vm);
/* Definition for a mem map lookup */
struct vm_lu_mem_map {
	struct list_head list;                 /* EPT mem map lookup list*/
	void *hpa;	/* Host physical start address of the map*/
	void *gpa;	/* Guest physical start address of the map */
	uint64_t size;	/* Size of map */
};

/* Use # of paging level to identify paging mode */
enum vm_paging_mode {
	PAGING_MODE_0_LEVEL = 0U,	/* Flat */
	PAGING_MODE_2_LEVEL = 2U,	/* 32bit paging, 2-level */
	PAGING_MODE_3_LEVEL = 3U,	/* PAE paging, 3-level */
	PAGING_MODE_4_LEVEL = 4U,	/* 64bit paging, 4-level */
	PAGING_MODE_NUM,
};

/*
 * VM related APIs
 */
uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask);

int gva2gpa(struct acrn_vcpu *vcpu, uint64_t gva, uint64_t *gpa, uint32_t *err_code);

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu);

int rdmsr_vmexit_handler(struct acrn_vcpu *vcpu);
int wrmsr_vmexit_handler(struct acrn_vcpu *vcpu);
void init_msr_emulation(struct acrn_vcpu *vcpu);

uint32_t vmsr_get_guest_msr_index(uint32_t msr);

struct run_context;
int vmx_vmrun(struct run_context *context, int ops, int ibrs);

int general_sw_loader(struct acrn_vm *vm);

typedef int (*vm_sw_loader_t)(struct acrn_vm *vm);
extern vm_sw_loader_t vm_sw_loader;
/**
 * @brief Data transfering between hypervisor and VM
 *
 * @defgroup acrn_mem ACRN Memory Management
 * @{
 */
/**
 * @brief Copy data from VM GPA space to HV address space
 *
 * @param[in] vm The pointer that points to VM data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *                  of HV memory region which data is stored in
 * @param[out] gpa The start GPA address of GPA memory region which data
 *                 will be copied into
 * @param[in] size The size (bytes) of GPA memory region which data is
 *                 stored in
 *
 * @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);
/**
 * @brief Copy data from HV address space to VM GPA space
 *
 * @param[in] vm The pointer that points to VM data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *                  of HV memory region which data is stored in
 * @param[out] gpa The start GPA address of GPA memory region which data
 *                 will be copied into
 * @param[in] size The size (bytes) of GPA memory region which data will be
 *                 copied into
 *
 * @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);
/**
 * @brief Copy data from VM GVA space to HV address space
 *
 * @param[in] vcpu The pointer that points to vcpu data structure
 * @param[out] h_ptr The pointer that returns the start HV address
 *                   of HV memory region which data will be copied to
 * @param[in] gva The start GVA address of GVA memory region which data
 *                is stored in
 * @param[in] size The size (bytes) of GVA memory region which data is
 *                 stored in
 * @param[out] err_code The page fault flags
 * @param[out] fault_addr The GVA address that causes a page fault
 */
int copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr);
/**
 * @brief Copy data from HV address space to VM GVA space
 *
 * @param[in] vcpu The pointer that points to vcpu data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *                   of HV memory region which data is stored in
 * @param[out] gva The start GVA address of GVA memory region which data
 *                will be copied into
 * @param[in] size The size (bytes) of GVA memory region which data will
 *                 be copied into
 * @param[out] err_code The page fault flags
 * @param[out] fault_addr The GVA address that causes a page fault
 */
int copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr);
/**
 * @}
 */
#endif	/* !ASSEMBLER */

#endif /* GUEST_H*/
