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

#ifndef INSTR_EMUL_H
#define INSTR_EMUL_H

#include <types.h>
#include <x86/cpu.h>
#include <x86/guest/guest_memory.h>

struct acrn_vcpu;
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
	uint64_t	gva;		/* saved gva for instruction emulation */
};

struct instr_emul_ctxt {
	struct instr_emul_vie vie;
};

int32_t emulate_instruction(struct acrn_vcpu *vcpu);
int32_t decode_instruction(struct acrn_vcpu *vcpu);
bool is_current_opcode_xchg(struct acrn_vcpu *vcpu);

#endif
