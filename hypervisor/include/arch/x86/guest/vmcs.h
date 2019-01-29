/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCS_H_
#define VMCS_H_

#define VM_SUCCESS	0
#define VM_FAIL		-1

#ifndef ASSEMBLER

#define VMX_VMENTRY_FAIL	0x80000000U

static inline uint32_t vmx_eoi_exit(uint32_t vector)
{
	return (VMX_EOI_EXIT0_FULL + ((vector >> 6U) * 2U));
}

/* VM exit qulifications for APIC-access
 * Access type:
 *  0  = linear access for a data read during instruction execution
 *  1  = linear access for a data write during instruction execution
 *  2  = linear access for an instruction fetch
 *  3  = linear access (read or write) during event delivery
 *  10 = guest-physical access during event delivery
 *  15 = guest-physical access for an instructon fetch or during
 *       instruction execution
 */
static inline uint64_t apic_access_type(uint64_t qual)
{
	return ((qual >> 12U) & 0xFUL);
}

static inline uint64_t apic_access_offset(uint64_t qual)
{
	return (qual & 0xFFFUL);
}

#define RFLAGS_C (1U<<0U)
#define RFLAGS_Z (1U<<6U)
#define RFLAGS_AC (1U<<18U)

#define VMX_SUPPORT_UNRESTRICTED_GUEST (1U<<5U)

void init_vmcs(struct acrn_vcpu *vcpu);

uint64_t vmx_rdmsr_pat(const struct acrn_vcpu *vcpu);
int32_t vmx_wrmsr_pat(struct acrn_vcpu *vcpu, uint64_t value);

void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);

static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.cpu_mode;
}

#endif /* ASSEMBLER */

#endif /* VMCS_H_ */
