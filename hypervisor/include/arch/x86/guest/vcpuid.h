/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCPUID_H_
#define VCPUID_H_

int32_t set_vcpuid_entries(struct acrn_vm *vm);
void guest_cpuid(struct acrn_vcpu *vcpu,
			uint32_t *eax, uint32_t *ebx,
			uint32_t *ecx, uint32_t *edx);

#endif /* VCPUID_H_ */
