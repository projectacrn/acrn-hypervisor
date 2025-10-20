/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VSBI_H
#define RISCV_VSBI_H

#include <types.h>

#define MAX_NUM_SUPPORTED_VSBI_EXT 8

#define MAX_VSBI_EXTENSION_NAME 16

struct vsbi_ret {
	uint64_t value;
	bool vcpu_retain_pc;
};

struct acrn_vsbi_extension {
	const char *name;
	uint64_t eid_start;
	uint64_t eid_end;
	int32_t (*handler)(struct acrn_vcpu *vcpu, uint64_t eid,
			uint64_t fid, uint64_t *args, struct vsbi_ret *out);
	int32_t (*probe)(struct acrn_vm *vm);
};

int32_t vsbi_exit_handler(struct acrn_vcpu *vcpu);
void init_vsbi(struct acrn_vm *vm);

#endif /* RISCV_VSBI_H */
