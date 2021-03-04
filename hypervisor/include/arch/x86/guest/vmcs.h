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
#include <types.h>
#include <x86/guest/vcpu.h>

#define VMX_VMENTRY_FAIL                0x80000000U

#define APIC_ACCESS_OFFSET              0xFFFUL   /* 11:0, offset within the APIC page */
#define APIC_ACCESS_TYPE                0xF000UL  /* 15:12, access type */
#define TYPE_LINEAR_APIC_INST_READ      (0UL << 12U)
#define TYPE_LINEAR_APIC_INST_WRITE     (1UL << 12U)

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
	return (qual & APIC_ACCESS_TYPE);
}

static inline uint64_t apic_access_offset(uint64_t qual)
{
	return (qual & APIC_ACCESS_OFFSET);
}
void init_vmcs(struct acrn_vcpu *vcpu);
void load_vmcs(const struct acrn_vcpu *vcpu);

void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu);
#endif /* ASSEMBLER */

#endif /* VMCS_H_ */
