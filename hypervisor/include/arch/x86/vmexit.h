/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VMEXIT_H_
#define VMEXIT_H_

struct vm_exit_dispatch {
	int (*handler)(struct vcpu *);
	uint32_t need_exit_qualification;
};

int vmexit_handler(struct vcpu *vcpu);
int vmcall_vmexit_handler(struct vcpu *vcpu);
int cpuid_vmexit_handler(struct vcpu *vcpu);
int cr_access_vmexit_handler(struct vcpu *vcpu);

#define VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, MSB, LSB)   \
	(exit_qual & (((1UL << (MSB+1U))-1UL) - ((1UL << (LSB))-1UL)))


/* MACROs to access Control-Register Info using exit qualification field */
#define VM_EXIT_CR_ACCESS_CR_NUM(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 3U, 0U) >> 0U)
#define VM_EXIT_CR_ACCESS_ACCESS_TYPE(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 5U, 4U) >> 4U)
#define VM_EXIT_CR_ACCESS_LMSW_OP(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 6U, 6U) >> 6U)
#define VM_EXIT_CR_ACCESS_REG_IDX(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 11U, 8U) >> 8U)
#define VM_EXIT_CR_ACCESS_LMSW_SRC_DATE(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 31U, 16U) >> 16U)

/* MACROs to access IO Access Info using exit qualification field */
#define VM_EXIT_IO_INSTRUCTION_SIZE(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 2U, 0U) >> 0U)
#define VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 3U, 3U) >> 3U)
#define VM_EXIT_IO_INSTRUCTION_IS_STRING(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 4U, 4U) >> 4U)
#define VM_EXIT_IO_INSTRUCTION_IS_REP_PREFIXED(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 5U, 5U) >> 5U)
#define VM_EXIT_IO_INSTRUCTION_IS_OPERAND_ENCODING(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 6U, 6U) >> 6U)
#define VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 31U, 16U) >> 16U)

#ifdef HV_DEBUG
void get_vmexit_profile(char *str, int str_max);
#endif /* HV_DEBUG */

#endif /* VMEXIT_H_ */
