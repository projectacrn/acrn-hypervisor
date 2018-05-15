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

#include <cpu.h>

struct vie_op {
	uint8_t		op_byte;	/* actual opcode byte */
	uint8_t		op_type;	/* type of operation (e.g. MOV) */
	uint16_t	op_flags;
};

#define	VIE_INST_SIZE	15
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
	int		base_register;		/* VM_REG_GUEST_xyz */
	int		index_register;		/* VM_REG_GUEST_xyz */
	int		segment_register;	/* VM_REG_GUEST_xyz */

	int64_t		displacement;		/* optional addr displacement */
	int64_t		immediate;		/* optional immediate operand */

	uint8_t		decoded;	/* set to 1 if successfully decoded */

	struct vie_op	op;			/* opcode description */
};

#define	PSL_C		0x00000001	/* carry bit */
#define	PSL_PF		0x00000004	/* parity bit */
#define	PSL_AF		0x00000010	/* bcd carry bit */
#define	PSL_Z		0x00000040	/* zero bit */
#define	PSL_N		0x00000080	/* negative bit */
#define	PSL_T		0x00000100	/* trace enable bit */
#define	PSL_I		0x00000200	/* interrupt enable bit */
#define	PSL_D		0x00000400	/* string instruction direction bit */
#define	PSL_V		0x00000800	/* overflow bit */
#define	PSL_IOPL	0x00003000	/* i/o privilege level */
#define	PSL_NT		0x00004000	/* nested task bit */
#define	PSL_RF		0x00010000	/* resume flag bit */
#define	PSL_VM		0x00020000	/* virtual 8086 mode bit */
#define	PSL_AC		0x00040000	/* alignment checking */
#define	PSL_VIF		0x00080000	/* virtual interrupt enable */
#define	PSL_VIP		0x00100000	/* virtual interrupt pending */
#define	PSL_ID		0x00200000	/* identification bit */

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
#define	PROT_NONE	0x00	/* no permissions */
#define	PROT_READ	0x01	/* pages can be read */
#define	PROT_WRITE	0x02	/* pages can be written */
#define	PROT_EXEC	0x04	/* pages can be executed */

#define	SEG_DESC_TYPE(access)		((access) & 0x001f)
#define	SEG_DESC_DPL(access)		(((access) >> 5) & 0x3)
#define	SEG_DESC_PRESENT(access)	(((access) & 0x0080) ? 1 : 0)
#define	SEG_DESC_DEF32(access)		(((access) & 0x4000) ? 1 : 0)
#define	SEG_DESC_GRANULARITY(access)	(((access) & 0x8000) ? 1 : 0)
#define	SEG_DESC_UNUSABLE(access)	(((access) & 0x10000) ? 1 : 0)

struct vm_guest_paging {
	uint64_t	cr3;
	int		cpl;
	enum vm_cpu_mode cpu_mode;
	enum vm_paging_mode paging_mode;
};

/*
 * Identifiers for architecturally defined registers.
 */
enum vm_reg_name {
	VM_REG_GUEST_RAX,
	VM_REG_GUEST_RBX,
	VM_REG_GUEST_RCX,
	VM_REG_GUEST_RDX,
	VM_REG_GUEST_RBP,
	VM_REG_GUEST_RSI,
	VM_REG_GUEST_R8,
	VM_REG_GUEST_R9,
	VM_REG_GUEST_R10,
	VM_REG_GUEST_R11,
	VM_REG_GUEST_R12,
	VM_REG_GUEST_R13,
	VM_REG_GUEST_R14,
	VM_REG_GUEST_R15,
	VM_REG_GUEST_RDI,
	VM_REG_GUEST_CR0,
	VM_REG_GUEST_CR3,
	VM_REG_GUEST_CR4,
	VM_REG_GUEST_DR7,
	VM_REG_GUEST_RSP,
	VM_REG_GUEST_RIP,
	VM_REG_GUEST_RFLAGS,
	VM_REG_GUEST_ES,
	VM_REG_GUEST_CS,
	VM_REG_GUEST_SS,
	VM_REG_GUEST_DS,
	VM_REG_GUEST_FS,
	VM_REG_GUEST_GS,
	VM_REG_GUEST_LDTR,
	VM_REG_GUEST_TR,
	VM_REG_GUEST_IDTR,
	VM_REG_GUEST_GDTR,
	VM_REG_GUEST_EFER,
	VM_REG_GUEST_CR2,
	VM_REG_GUEST_PDPTE0,
	VM_REG_GUEST_PDPTE1,
	VM_REG_GUEST_PDPTE2,
	VM_REG_GUEST_PDPTE3,
	VM_REG_GUEST_INTR_SHADOW,
	VM_REG_LAST
};

typedef unsigned long u_long;

int vm_get_register(struct vcpu *vcpu, int reg, uint64_t *retval);
int vm_set_register(struct vcpu *vcpu, int reg, uint64_t val);
int vm_get_seg_desc(struct vcpu *vcpu, int reg,
		struct seg_desc *ret_desc);
int vm_set_seg_desc(struct vcpu *vcpu, int reg,
		struct seg_desc *desc);
int vm_restart_instruction(struct vcpu *vcpu);
