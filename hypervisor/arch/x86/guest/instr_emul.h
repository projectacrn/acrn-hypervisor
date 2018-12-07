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

/**
 * Define the following MACRO to make range checking clear.
 *
 * CPU_REG_FIRST indicates the first register name, its value
 * is the same as CPU_REG_RAX;
 * CPU_REG_LAST indicates the last register name, its value is
 * the same as CPU_REG_GDTR;
 *
 * CPU_REG_GENERAL_FIRST indicates the first general register name,
 * its value is the same as CPU_REG_RAX;
 * CPU_REG_GENERAL_LAST indicates the last general register name,
 * its value is the same as CPU_REG_RDI;
 *
 * CPU_REG_NONGENERAL_FIRST indicates the first non general register
 * name, its value is the same as CPU_REG_CR0;
 * CPU_REG_NONGENERAL_LAST indicates the last non general register
 * name, its value is the same as CPU_REG_GDTR;
 *
 * CPU_REG_NATURAL_FIRST indicates the first register name that
 * is corresponds to the natural width field in VMCS, its value
 * is the same as CPU_REG_CR0;
 * CPU_REG_NATURAL_LAST indicates the last register name that
 * is corresponds to the natural width field in VMCS, its value
 * is the same as CPU_REG_RFLAGS;
 *
 * CPU_REG_64BIT_FIRST indicates the first register name that
 * is corresponds to the 64 bit field in VMCS, its value
 * is the same as CPU_REG_EFER;
 * CPU_REG_64BIT_LAST indicates the last register name that
 * is corresponds to the 64 bit field in VMCS, its value
 * is the same as CPU_REG_PDPTE3;
 *
 * CPU_REG_SEG_FIRST indicates the first segement register name,
 * its value is the same as CPU_REG_ES;
 * CPU_REG_SEG_FIRST indicates the last segement register name,
 * its value is the same as CPU_REG_GS
 *
 */
#define CPU_REG_FIRST			CPU_REG_RAX
#define CPU_REG_LAST            	CPU_REG_GDTR
#define CPU_REG_GENERAL_FIRST   	CPU_REG_RAX
#define CPU_REG_GENERAL_LAST		CPU_REG_R15
#define CPU_REG_NONGENERAL_FIRST   	CPU_REG_CR0
#define CPU_REG_NONGENERAL_LAST   	CPU_REG_GDTR
#define CPU_REG_NATURAL_FIRST		CPU_REG_CR0
#define CPU_REG_NATURAL_LAST		CPU_REG_RFLAGS
#define CPU_REG_64BIT_FIRST		CPU_REG_EFER
#define CPU_REG_64BIT_LAST 		CPU_REG_PDPTE3
#define CPU_REG_SEG_FIRST		CPU_REG_ES
#define CPU_REG_SEG_LAST		CPU_REG_GS

struct instr_emul_vie_op {
	uint8_t		op_type;	/* type of operation (e.g. MOV) */
	uint16_t	op_flags;
};

#define VIE_PREFIX_SIZE	4U
#define VIE_INST_SIZE	15U
struct instr_emul_vie {
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
			seg_override:1;	/* Segment override */

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

	uint8_t		opcode;
	struct instr_emul_vie_op	op;			/* opcode description */

	uint64_t	dst_gpa;	/* saved dst operand gpa. Only for movs */
};

#define	PSL_C		0x00000001U	/* carry bit */
#define	PSL_PF		0x00000004U	/* parity bit */
#define	PSL_AF		0x00000010U	/* bcd carry bit */
#define	PSL_Z		0x00000040U	/* zero bit */
#define	PSL_N		0x00000080U	/* negative bit */
#define	PSL_D		0x00000400U	/* string instruction direction bit */
#define	PSL_V		0x00000800U	/* overflow bit */
#define	PSL_AC		0x00040000U	/* alignment checking */

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
#define	PROT_READ	0x01U	/* pages can be read */
#define	PROT_WRITE	0x02U	/* pages can be written */

static inline uint32_t seg_desc_type(uint32_t access)
{
	return (access & 0x001fU);
}

static inline bool seg_desc_present(uint32_t access)
{
	return ((access & 0x0080U) != 0U);
}

static inline bool seg_desc_def32(uint32_t access)
{
	return ((access & 0x4000U) != 0U);
}

static inline bool seg_desc_unusable(uint32_t access)
{
	return ((access & 0x10000U) != 0U);
}

struct vm_guest_paging {
	uint64_t	cr3;
	uint8_t		cpl;
	enum vm_cpu_mode cpu_mode;
	enum vm_paging_mode paging_mode;
};

struct instr_emul_ctxt {
	struct instr_emul_vie vie;
	struct vm_guest_paging paging;
	struct acrn_vcpu *vcpu;
};

int32_t emulate_instruction(const struct acrn_vcpu *vcpu);
int32_t decode_instruction(struct acrn_vcpu *vcpu);

#endif
