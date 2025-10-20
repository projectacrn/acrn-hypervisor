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

#define INVALID_PIO_IDX	-1U
#define UART_PIO_IDX0	INVALID_PIO_IDX
/* FIXME: dummy. to be implemented later */
#define EMUL_PIO_IDX_MAX 1U

struct vm_arch {
	const struct acrn_vsbi_extension *vsbi_exts[MAX_NUM_SUPPORTED_VSBI_EXT];
	uint16_t n_vsbi_exts;
};

struct acrn_vcpu;
struct acrn_vm;
uint32_t vcpu_get_vhartid(struct acrn_vcpu *vcpu);
struct acrn_vcpu *vcpu_from_vhartid(struct acrn_vm *vm, uint32_t vhartid);

#endif /* RISCV_VM_H_ */
