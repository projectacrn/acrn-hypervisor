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
 * This VMCS12 revision id is chosen arbitrarily.
 * The emulated MSR_IA32_VMX_BASIC returns this ID in bits 30:0.
 */
#define VMCS12_REVISION_ID		0x15407E12U

enum VMXResult {
	VMsucceed,
	VMfailValid,
	VMfailInvalid,
};
void nested_vmx_result(enum VMXResult, int error_number);
int32_t vmxon_vmexit_handler(struct acrn_vcpu *vcpu);

#ifdef CONFIG_NVMX_ENABLED
struct acrn_nested {
	bool vmxon;		/* To indicate if vCPU entered VMX operation */
} __aligned(PAGE_SIZE);

bool is_vmx_msr(uint32_t msr);
void init_vmx_msrs(struct acrn_vcpu *vcpu);
int32_t read_vmx_msr(__unused struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val);
#else
struct acrn_nested {};

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
