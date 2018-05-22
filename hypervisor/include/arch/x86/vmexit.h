/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
int get_vmexit_profile(char *str, int str_max);

#define VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, MSB, LSB)   \
	(exit_qual & (((1UL << (MSB+1))-1) - ((1UL << (LSB))-1)))


/* MACROs to access Control-Register Info using exit qualification field */
#define VM_EXIT_CR_ACCESS_CR_NUM(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 3, 0) >> 0)
#define VM_EXIT_CR_ACCESS_ACCESS_TYPE(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 5, 4) >> 4)
#define VM_EXIT_CR_ACCESS_LMSW_OP(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 6, 6) >> 6)
#define VM_EXIT_CR_ACCESS_REG_IDX(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 11, 8) >> 8)
#define VM_EXIT_CR_ACCESS_LMSW_SRC_DATE(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 31, 16) >> 16)

/* MACROs to access IO Access Info using exit qualification field */
#define VM_EXIT_IO_INSTRUCTION_SIZE(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 2, 0) >> 0)
#define VM_EXIT_IO_INSTRUCTION_ACCESS_DIRECTION(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 3, 3) >> 3)
#define VM_EXIT_IO_INSTRUCTION_IS_STRING(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 4, 4) >> 4)
#define VM_EXIT_IO_INSTRUCTION_IS_REP_PREFIXED(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 5, 5) >> 5)
#define VM_EXIT_IO_INSTRUCTION_IS_OPERAND_ENCODING(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 6, 6) >> 6)
#define VM_EXIT_IO_INSTRUCTION_PORT_NUMBER(exit_qual) \
	(VM_EXIT_QUALIFICATION_BIT_MASK(exit_qual, 31, 16) >> 16)

#endif /* VMEXIT_H_ */
