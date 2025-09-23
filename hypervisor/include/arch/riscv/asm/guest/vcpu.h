/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VCPU_H
#define RISCV_VCPU_H


#include <cpu.h>
#include <io_req.h>


struct acrn_vm;
struct acrn_vcpu {
	struct acrn_vm *vm;		/* Reference to the VM this VCPU belongs to */
	struct io_request req; /* used by io/ept emulation */

};

static inline uint16_t pcpuid_from_vcpu(__unused const struct acrn_vcpu *vcpu)
{
	return INVALID_CPU_ID;
}

#endif /* RISCV_VCPU_H */
