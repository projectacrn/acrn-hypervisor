/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCPUID_H_
#define VCPUID_H_

#define CPUID_CHECK_SUBLEAF	(1U << 0U)
#define MAX_VM_VCPUID_ENTRIES	64U

/* Guest capability flags reported by CPUID */
#define GUEST_CAPS_PRIVILEGE_VM	(1U << 0U)

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

int32_t set_vcpuid_entries(struct acrn_vm *vm);
void guest_cpuid(struct acrn_vcpu *vcpu,
			uint32_t *eax, uint32_t *ebx,
			uint32_t *ecx, uint32_t *edx);

#endif /* VCPUID_H_ */
