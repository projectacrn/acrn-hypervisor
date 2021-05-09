/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef NESTED_H
#define NESTED_H

#include <lib/errno.h>

/* helper data structure to make VMX capability MSR manipulation easier */
union value_64 {
	uint64_t full;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	} u;
};

/*
 * Following MSRs are supported if nested virtualization is enabled
 * - If CONFIG_NVMX_ENABLED is set, these MSRs are included in emulated_guest_msrs[]
 * - otherwise, they are included in unsupported_msrs[]
 */
#define NUM_VMX_MSRS	20U
#define LIST_OF_VMX_MSRS	\
	MSR_IA32_SMBASE,	\
	MSR_IA32_VMX_BASIC,			\
	MSR_IA32_VMX_PINBASED_CTLS,		\
	MSR_IA32_VMX_PROCBASED_CTLS,		\
	MSR_IA32_VMX_EXIT_CTLS,			\
	MSR_IA32_VMX_ENTRY_CTLS,		\
	MSR_IA32_VMX_MISC,			\
	MSR_IA32_VMX_CR0_FIXED0,		\
	MSR_IA32_VMX_CR0_FIXED1,		\
	MSR_IA32_VMX_CR4_FIXED0,		\
	MSR_IA32_VMX_CR4_FIXED1,		\
	MSR_IA32_VMX_VMCS_ENUM,			\
	MSR_IA32_VMX_PROCBASED_CTLS2,		\
	MSR_IA32_VMX_EPT_VPID_CAP,		\
	MSR_IA32_VMX_TRUE_PINBASED_CTLS,	\
	MSR_IA32_VMX_TRUE_PROCBASED_CTLS,	\
	MSR_IA32_VMX_TRUE_EXIT_CTLS,		\
	MSR_IA32_VMX_TRUE_ENTRY_CTLS,		\
	MSR_IA32_VMX_VMFUNC,			\
	MSR_IA32_VMX_PROCBASED_CTLS3

/*
 * VM-Exit Instruction-Information Field
 *
 * ISDM Vol 3C Table 27-9: INVEPT, INVPCID, INVVPID
 * ISDM Vol 3C Table 27-13: VMCLEAR, VMPTRLD, VMPTRST, VMXON, XRSTORS, and XSAVES.
 * ISDM Vol 3C Table 27-14: VMREAD and VMWRITE
 *
 * Either Table 27-9 or Table 27-13 is a subset of Table 27-14, so we are able to
 * define the following macros to be used for the above mentioned instructions.
 */
#define VMX_II_SCALING(v)			(((v) >> 0U) & 0x3U)
#define VMX_II_REG1(v)				(((v) >> 3U) & 0xfU)
#define VMX_II_ADDR_SIZE(v)			(((v) >> 7U) & 0x7U)
#define VMX_II_IS_REG(v)			(((v) >> 10U) & 0x1U)
#define VMX_II_SEG_REG(v)			(((v) >> 15U) & 0x7U)
#define VMX_II_IDX_REG(v)			(((v) >> 18U) & 0xfU)
#define VMX_II_IDX_REG_VALID(v)			((((v) >> 22U) & 0x1U) == 0U)
#define VMX_II_BASE_REG(v)			(((v) >> 23U) & 0xfU)
#define VMX_II_BASE_REG_VALID(v)		((((v) >> 27U) & 0x1U) == 0U)
#define VMX_II_REG2(v)				(((v) >> 28U) & 0xfU)

#define VMCS_SHADOW_BIT_INDICATOR		(1U << 31U)

/* refer to ISDM: Table 30-1. VM-Instruction Error Numbers */
#define VMXERR_VMPTRLD_INVALID_ADDRESS		(9)
#define VMXERR_VMPTRLD_INCORRECT_VMCS_REVISION_ID (10)
#define VMXERR_VMPTRLD_VMXON_POINTER		(11)
#define VMXERR_VMXON_IN_VMX_ROOT_OPERATION	(15)

/*
 * This VMCS12 revision id is chosen arbitrarily.
 * The emulated MSR_IA32_VMX_BASIC returns this ID in bits 30:0.
 */
#define VMCS12_REVISION_ID		0x15407E12U

/* Implemented in next patch */
struct acrn_vmcs12 {};

enum VMXResult {
	VMsucceed,
	VMfailValid,
	VMfailInvalid,
};
void nested_vmx_result(enum VMXResult, int error_number);
int32_t vmxon_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmxoff_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmptrld_vmexit_handler(struct acrn_vcpu *vcpu);

#ifdef CONFIG_NVMX_ENABLED
struct acrn_nested {
	uint8_t vmcs02[PAGE_SIZE];	/* VMCS to run L2 and as Link Pointer in VMCS01 */
	struct acrn_vmcs12 vmcs12;	/* To cache L1's VMCS12*/
	uint64_t current_vmcs12_ptr;	/* GPA */
	uint64_t vmxon_ptr;		/* GPA */
	bool vmxon;		/* To indicate if vCPU entered VMX operation */
} __aligned(PAGE_SIZE);

void init_nested_vmx(__unused struct acrn_vm *vm);
bool is_vmx_msr(uint32_t msr);
void init_vmx_msrs(struct acrn_vcpu *vcpu);
int32_t read_vmx_msr(__unused struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val);
#else
struct acrn_nested {};

static inline void init_nested_vmx(__unused struct acrn_vm *vm) {}
static inline bool is_vmx_msr(__unused uint32_t msr)
{
	/*
	 * if nested virtualization is disabled, return false so that
	 * it can be treated as unsupported MSR.
	 */
	return false;
}

static inline void init_vmx_msrs(__unused struct acrn_vcpu *vcpu) {}

static inline int32_t read_vmx_msr(__unused struct acrn_vcpu *vcpu,
	__unused uint32_t msr, __unused uint64_t *val)
{
	return -EACCES;
}
#endif /* CONFIG_NVMX_ENABLED */
#endif /* NESTED_H */
