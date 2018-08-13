/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MTRR_H
#define MTRR_H

#define FIXED_RANGE_MTRR_NUM	11U
#define MTRR_SUB_RANGE_NUM		8U

union mtrr_cap_reg {
	uint64_t value;
	struct {
		uint32_t vcnt:8;
		uint32_t fix:1;
		uint32_t res0:1;
		uint32_t wc:1;
		uint32_t res1:21;
		uint32_t res2:32;
	} bits;
};

union mtrr_def_type_reg {
	uint64_t value;
	struct {
		uint32_t type:8;
		uint32_t res0:2;
		uint32_t fixed_enable:1;
		uint32_t enable:1;
		uint32_t res1:20;
		uint32_t res2:32;
	} bits;
};

union mtrr_fixed_range_reg {
	uint64_t value;
	uint8_t type[MTRR_SUB_RANGE_NUM];
};

struct mtrr_state {
	union mtrr_cap_reg			cap;
	union mtrr_def_type_reg		def_type;
	union mtrr_fixed_range_reg	fixed_range[FIXED_RANGE_MTRR_NUM];
};

void mtrr_wrmsr(struct vcpu *vcpu, uint32_t msr, uint64_t value);
uint64_t mtrr_rdmsr(struct vcpu *vcpu, uint32_t msr);
void init_mtrr(struct vcpu *vcpu);

#endif /* MTRR_H */
