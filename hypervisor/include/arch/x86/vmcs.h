/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMCS_H_
#define VMCS_H_

#define VM_SUCCESS	0
#define VM_FAIL		-1

#define VMX_VMENTRY_FAIL	0x80000000U

#ifndef ASSEMBLER

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

/* CR0 bits hv want to trap to track status change */
#define CR0_TRAP_MASK (CR0_PE | CR0_PG | CR0_WP | CR0_CD | CR0_NW )
#define CR0_RESERVED_MASK ~(CR0_PG | CR0_CD | CR0_NW | CR0_AM | CR0_WP | \
			   CR0_NE |  CR0_ET | CR0_TS | CR0_EM | CR0_MP | CR0_PE)

/* CR4 bits hv want to trap to track status change */
#define CR4_TRAP_MASK (CR4_PSE | CR4_PAE | CR4_VMXE | CR4_PCIDE)
#define	CR4_RESERVED_MASK ~(CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | \
				CR4_PAE | CR4_MCE | CR4_PGE | CR4_PCE |	     \
				CR4_OSFXSR | CR4_PCIDE | CR4_OSXSAVE |       \
				CR4_SMEP | CR4_FSGSBASE | CR4_VMXE |         \
				CR4_OSXMMEXCPT | CR4_SMAP | CR4_PKE |        \
				CR4_SMXE | CR4_UMIP )

#define VMX_SUPPORT_UNRESTRICTED_GUEST (1U<<5U)

void init_vmcs(struct acrn_vcpu *vcpu);

uint64_t vmx_rdmsr_pat(const struct acrn_vcpu *vcpu);
int32_t vmx_wrmsr_pat(struct acrn_vcpu *vcpu, uint64_t value);

void vmx_write_cr0(struct acrn_vcpu *vcpu, uint64_t cr0);
void vmx_write_cr4(struct acrn_vcpu *vcpu, uint64_t cr4);
bool is_vmx_disabled(void);
void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);

static inline enum vm_cpu_mode get_vcpu_mode(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.cpu_mode;
}

static inline bool cpu_has_vmx_unrestricted_guest_cap(void)
{
	return ((msr_read(MSR_IA32_VMX_MISC) & VMX_SUPPORT_UNRESTRICTED_GUEST)
									!= 0UL);
}

#endif /* ASSEMBLER */

#endif /* VMCS_H_ */
