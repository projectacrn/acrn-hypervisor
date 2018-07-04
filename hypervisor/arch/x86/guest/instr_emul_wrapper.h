/*-
 * Copyright (c) 2012 NetApp, Inc.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef INSTR_EMUL_WRAPPER_H
#define INSTR_EMUL_WRAPPER_H
#include <cpu.h>

/*
 * Identifiers for architecturally defined registers.
 */
enum cpu_reg_name {
	CPU_REG_RAX,
	CPU_REG_RBX,
	CPU_REG_RCX,
	CPU_REG_RDX,
	CPU_REG_RBP,
	CPU_REG_RSI,
	CPU_REG_R8,
	CPU_REG_R9,
	CPU_REG_R10,
	CPU_REG_R11,
	CPU_REG_R12,
	CPU_REG_R13,
	CPU_REG_R14,
	CPU_REG_R15,
	CPU_REG_RDI,
	CPU_REG_CR0,
	CPU_REG_CR3,
	CPU_REG_CR4,
	CPU_REG_DR7,
	CPU_REG_RSP,
	CPU_REG_RIP,
	CPU_REG_RFLAGS,
	CPU_REG_ES,
	CPU_REG_CS,
	CPU_REG_SS,
	CPU_REG_DS,
	CPU_REG_FS,
	CPU_REG_GS,
	CPU_REG_LDTR,
	CPU_REG_TR,
	CPU_REG_IDTR,
	CPU_REG_GDTR,
	CPU_REG_EFER,
	CPU_REG_CR2,
	CPU_REG_PDPTE0,
	CPU_REG_PDPTE1,
	CPU_REG_PDPTE2,
	CPU_REG_PDPTE3,
	CPU_REG_LAST
};

struct vie_op {
	uint8_t		op_byte;	/* actual opcode byte */
	uint8_t		op_type;	/* type of operation (e.g. MOV) */
	uint16_t	op_flags;
};

#define	VIE_INST_SIZE	15U
struct vie {
	uint8_t		inst[VIE_INST_SIZE];	/* instruction bytes */
	uint8_t		num_valid;		/* size of the instruction */
	uint8_t		num_processed;

	uint8_t		addrsize:4, opsize:4;	/* address and operand sizes */
	uint8_t		rex_w:1,		/* REX prefix */
			rex_r:1,
			rex_x:1,
			rex_b:1,
			rex_present:1,
			repz_present:1,		/* REP/REPE/REPZ prefix */
			repnz_present:1,	/* REPNE/REPNZ prefix */
			opsize_override:1,	/* Operand size override */
			addrsize_override:1,	/* Address size override */
			segment_override:1;	/* Segment override */

	uint8_t		mod:2,			/* ModRM byte */
			reg:4,
			rm:4;

	uint8_t		ss:2,			/* SIB byte */
			index:4,
			base:4;

	uint8_t		disp_bytes;
	uint8_t		imm_bytes;

	uint8_t		scale;
	enum cpu_reg_name base_register;		/* CPU_REG_xyz */
	enum cpu_reg_name index_register;	/* CPU_REG_xyz */
	enum cpu_reg_name segment_register;	/* CPU_REG_xyz */

	int64_t		displacement;		/* optional addr displacement */
	int64_t		immediate;		/* optional immediate operand */

	uint8_t		decoded;	/* set to 1 if successfully decoded */

	struct vie_op	op;			/* opcode description */
};

#define	PSL_C		0x00000001U	/* carry bit */
#define	PSL_PF		0x00000004U	/* parity bit */
#define	PSL_AF		0x00000010U	/* bcd carry bit */
#define	PSL_Z		0x00000040U	/* zero bit */
#define	PSL_N		0x00000080U	/* negative bit */
#define	PSL_T		0x00000100U	/* trace enable bit */
#define	PSL_I		0x00000200U	/* interrupt enable bit */
#define	PSL_D		0x00000400U	/* string instruction direction bit */
#define	PSL_V		0x00000800U	/* overflow bit */
#define	PSL_IOPL	0x00003000U	/* i/o privilege level */
#define	PSL_NT		0x00004000U	/* nested task bit */
#define	PSL_RF		0x00010000U	/* resume flag bit */
#define	PSL_VM		0x00020000U	/* virtual 8086 mode bit */
#define	PSL_AC		0x00040000U	/* alignment checking */
#define	PSL_VIF		0x00080000U	/* virtual interrupt enable */
#define	PSL_VIP		0x00100000U	/* virtual interrupt pending */
#define	PSL_ID		0x00200000U	/* identification bit */

/*
 * The 'access' field has the format specified in Table 21-2 of the Intel
 * Architecture Manual vol 3b.
 *
 * XXX The contents of the 'access' field are architecturally defined except
 * bit 16 - Segment Unusable.
 */
struct seg_desc {
	uint64_t	base;
	uint32_t	limit;
	uint32_t	access;
};


/*
 * Protections are chosen from these bits, or-ed together
 */
#define	PROT_NONE	0x00U	/* no permissions */
#define	PROT_READ	0x01U	/* pages can be read */
#define	PROT_WRITE	0x02U	/* pages can be written */
#define	PROT_EXEC	0x04U	/* pages can be executed */

#define	SEG_DESC_TYPE(access)		((access) & 0x001fU)
#define	SEG_DESC_DPL(access)		(((access) >> 5) & 0x3U)
#define	SEG_DESC_PRESENT(access)	(((access) & 0x0080U) != 0U)
#define	SEG_DESC_DEF32(access)		(((access) & 0x4000U) != 0U)
#define	SEG_DESC_GRANULARITY(access)	(((access) & 0x8000U) != 0U)
#define	SEG_DESC_UNUSABLE(access)	(((access) & 0x10000U) != 0U)

struct vm_guest_paging {
	uint64_t	cr3;
	uint8_t		cpl;
	enum vm_cpu_mode cpu_mode;
	enum vm_paging_mode paging_mode;
};

struct emul_cnx {
	struct vie vie;
	struct vm_guest_paging paging;
	struct vcpu *vcpu;
};

int vm_get_register(struct vcpu *vcpu, enum cpu_reg_name reg, uint64_t *retval);
int vm_set_register(struct vcpu *vcpu, enum cpu_reg_name reg, uint64_t val);
int vm_get_seg_desc(struct vcpu *vcpu, enum cpu_reg_name reg,
		struct seg_desc *ret_desc);
int vm_set_seg_desc(struct vcpu *vcpu, enum cpu_reg_name reg,
		struct seg_desc *desc);
#endif
