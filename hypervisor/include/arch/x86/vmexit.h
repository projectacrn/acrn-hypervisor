/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMEXIT_H_
#define VMEXIT_H_

struct vm_exit_dispatch {
	int (*handler)(struct acrn_vcpu *);
	uint32_t need_exit_qualification;
};

int vmexit_handler(struct acrn_vcpu *vcpu);
int vmcall_vmexit_handler(struct acrn_vcpu *vcpu);
int cpuid_vmexit_handler(struct acrn_vcpu *vcpu);
int cr_access_vmexit_handler(struct acrn_vcpu *vcpu);
extern void vm_exit(void);
static inline uint64_t
vm_exit_qualification_bit_mask(uint64_t exit_qual, uint32_t msb, uint32_t lsb)
{
	return (exit_qual &
			(((1UL << (msb + 1U)) - 1UL) - ((1UL << lsb) - 1UL)));
}

/* access Control-Register Info using exit qualification field */
static inline uint64_t vm_exit_cr_access_cr_num(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 3U, 0U) >> 0U);
}

static inline uint64_t vm_exit_cr_access_type(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 5U, 4U) >> 4U);
}

static inline uint64_t vm_exit_cr_access_lmsw_op(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 6U, 6U) >> 6U);
}

static inline uint64_t vm_exit_cr_access_reg_idx(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 11U, 8U) >> 8U);
}

static inline uint64_t vm_exit_cr_access_lmsw_src_date(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 31U, 16U) >> 16U);
}

/* access IO Access Info using exit qualification field */
static inline uint64_t vm_exit_io_instruction_size(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 2U, 0U) >> 0U);
}

static inline uint64_t
vm_exit_io_instruction_access_direction(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 3U, 3U) >> 3U);
}

static inline uint64_t vm_exit_io_instruction_is_string(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 4U, 4U) >> 4U);
}

static inline uint64_t
vm_exit_io_instruction_is_rep_prefixed(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 5U, 5U) >> 5U);
}

static inline uint64_t
vm_exit_io_instruction_is_operand_encoding(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 6U, 6U) >> 6U);
}

static inline uint64_t vm_exit_io_instruction_port_number(uint64_t exit_qual)
{
	return (vm_exit_qualification_bit_mask(exit_qual, 31U, 16U) >> 16U);
}

#endif /* VMEXIT_H_ */
