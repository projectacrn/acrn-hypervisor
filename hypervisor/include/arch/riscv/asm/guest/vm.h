/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VM_H_
#define RISCV_VM_H_

#include <vm_configurations.h>
#include <vuart.h>
#include <fdt_api.h>

#include <asm/guest/vsbi.h>

struct vm_arch {
	const struct acrn_vsbi_extension *vsbi_exts[MAX_NUM_SUPPORTED_VSBI_EXT];
	uint16_t n_vsbi_exts;
	uint64_t mvendorid;
	uint64_t marchid;
	uint64_t mimpid;

	int64_t time_delta;
};

struct acrn_vcpu;
struct acrn_vm;
uint32_t vcpu_get_vhartid(struct acrn_vcpu *vcpu);
struct acrn_vcpu *vcpu_from_vhartid(struct acrn_vm *vm, uint32_t vhartid);

static inline void deny_guest_pio_access(struct acrn_vm *vm, uint16_t port_address, uint32_t nbytes)
{
	(void)vm;
	(void)port_address;
	(void)nbytes;
}

#endif /* RISCV_VM_H_ */
