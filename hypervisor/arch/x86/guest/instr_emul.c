/*-
 * Copyright (c) 2012 Sandvine, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <hypervisor.h>

#include "instr_emul_wrapper.h"
#include "instr_emul.h"

/* struct vie_op.op_type */
#define VIE_OP_TYPE_NONE	0U
#define VIE_OP_TYPE_MOV		1U
#define VIE_OP_TYPE_MOVSX	2U
#define VIE_OP_TYPE_MOVZX	3U
#define VIE_OP_TYPE_AND		4U
#define VIE_OP_TYPE_OR		5U
#define VIE_OP_TYPE_SUB		6U
#define VIE_OP_TYPE_TWO_BYTE	7U
#define VIE_OP_TYPE_PUSH	8U
#define VIE_OP_TYPE_CMP		9U
#define VIE_OP_TYPE_POP		10U
#define VIE_OP_TYPE_MOVS	11U
#define VIE_OP_TYPE_GROUP1	12U
#define VIE_OP_TYPE_STOS	13U
#define VIE_OP_TYPE_BITTEST	14U
#define VIE_OP_TYPE_TEST	15U

/* struct vie_op.op_flags */
#define	VIE_OP_F_IMM		(1U << 0)  /* 16/32-bit immediate operand */
#define	VIE_OP_F_IMM8		(1U << 1)  /* 8-bit immediate operand */
#define	VIE_OP_F_MOFFSET	(1U << 2)  /* 16/32/64-bit immediate moffset */
#define	VIE_OP_F_NO_MODRM	(1U << 3)
#define	VIE_OP_F_NO_GLA_VERIFICATION (1U << 4)

static const struct vie_op two_byte_opcodes[256] = {
	[0xB6] = {
		.op_byte = 0xB6,
		.op_type = VIE_OP_TYPE_MOVZX,
	},
	[0xB7] = {
		.op_byte = 0xB7,
		.op_type = VIE_OP_TYPE_MOVZX,
	},
	[0xBA] = {
		.op_byte = 0xBA,
		.op_type = VIE_OP_TYPE_BITTEST,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0xBE] = {
		.op_byte = 0xBE,
		.op_type = VIE_OP_TYPE_MOVSX,
	},
};

static const struct vie_op one_byte_opcodes[256] = {
	[0x0F] = {
		.op_byte = 0x0FU,
		.op_type = VIE_OP_TYPE_TWO_BYTE
	},
	[0x2B] = {
		.op_byte = 0x2BU,
		.op_type = VIE_OP_TYPE_SUB,
	},
	[0x39] = {
		.op_byte = 0x39U,
		.op_type = VIE_OP_TYPE_CMP,
	},
	[0x3B] = {
		.op_byte = 0x3BU,
		.op_type = VIE_OP_TYPE_CMP,
	},
	[0x88] = {
		.op_byte = 0x88U,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x89] = {
		.op_byte = 0x89U,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8A] = {
		.op_byte = 0x8AU,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8B] = {
		.op_byte = 0x8BU,
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0xA1] = {
		.op_byte = 0xA1U,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_MOFFSET | VIE_OP_F_NO_MODRM,
	},
	[0xA3] = {
		.op_byte = 0xA3U,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_MOFFSET | VIE_OP_F_NO_MODRM,
	},
	[0xA4] = {
		.op_byte = 0xA4U,
		.op_type = VIE_OP_TYPE_MOVS,
		.op_flags = VIE_OP_F_NO_MODRM | VIE_OP_F_NO_GLA_VERIFICATION
	},
	[0xA5] = {
		.op_byte = 0xA5U,
		.op_type = VIE_OP_TYPE_MOVS,
		.op_flags = VIE_OP_F_NO_MODRM | VIE_OP_F_NO_GLA_VERIFICATION
	},
	[0xAA] = {
		.op_byte = 0xAAU,
		.op_type = VIE_OP_TYPE_STOS,
		.op_flags = VIE_OP_F_NO_MODRM | VIE_OP_F_NO_GLA_VERIFICATION
	},
	[0xAB] = {
		.op_byte = 0xABU,
		.op_type = VIE_OP_TYPE_STOS,
		.op_flags = VIE_OP_F_NO_MODRM | VIE_OP_F_NO_GLA_VERIFICATION
	},
	[0xC6] = {
		/* XXX Group 11 extended opcode - not just MOV */
		.op_byte = 0xC6U,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0xC7] = {
		.op_byte = 0xC7U,
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_IMM,
	},
	[0x23] = {
		.op_byte = 0x23U,
		.op_type = VIE_OP_TYPE_AND,
	},
	[0x80] = {
		/* Group 1 extended opcode */
		.op_byte = 0x80U,
		.op_type = VIE_OP_TYPE_GROUP1,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0x81] = {
		/* Group 1 extended opcode */
		.op_byte = 0x81U,
		.op_type = VIE_OP_TYPE_GROUP1,
		.op_flags = VIE_OP_F_IMM,
	},
	[0x83] = {
		/* Group 1 extended opcode */
		.op_byte = 0x83U,
		.op_type = VIE_OP_TYPE_GROUP1,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0x84] = {
		.op_byte = 0x84U,
		.op_type = VIE_OP_TYPE_TEST,
	},
	[0x85] = {
		.op_byte = 0x85U,
		.op_type = VIE_OP_TYPE_TEST,
	},
	[0x08] = {
		.op_byte = 0x08U,
		.op_type = VIE_OP_TYPE_OR,
	},
	[0x09] = {
		.op_byte = 0x09U,
		.op_type = VIE_OP_TYPE_OR,
	},
	[0x8F] = {
		/* XXX Group 1A extended opcode - not just POP */
		.op_byte = 0x8FU,
		.op_type = VIE_OP_TYPE_POP,
	},
	[0xFF] = {
		/* XXX Group 5 extended opcode - not just PUSH */
		.op_byte = 0xFFU,
		.op_type = VIE_OP_TYPE_PUSH,
	}
};

/* struct vie.mod */
#define	VIE_MOD_INDIRECT		0U
#define	VIE_MOD_INDIRECT_DISP8		1U
#define	VIE_MOD_INDIRECT_DISP32		2U
#define	VIE_MOD_DIRECT			3U

/* struct vie.rm */
#define	VIE_RM_SIB			4U
#define	VIE_RM_DISP32			5U

#define	GB				(1024 * 1024 * 1024)

static enum cpu_reg_name gpr_map[16] = {
	CPU_REG_RAX,
	CPU_REG_RCX,
	CPU_REG_RDX,
	CPU_REG_RBX,
	CPU_REG_RSP,
	CPU_REG_RBP,
	CPU_REG_RSI,
	CPU_REG_RDI,
	CPU_REG_R8,
	CPU_REG_R9,
	CPU_REG_R10,
	CPU_REG_R11,
	CPU_REG_R12,
	CPU_REG_R13,
	CPU_REG_R14,
	CPU_REG_R15
};

static uint64_t size2mask[] = {
	[1] = 0xffUL,
	[2] = 0xffffUL,
	[4] = 0xffffffffUL,
	[8] = 0xffffffffffffffffUL,
};

static int
vie_read_register(struct vcpu *vcpu, enum cpu_reg_name reg, uint64_t *rval)
{
	int error;

	error = vm_get_register(vcpu, reg, rval);
	ASSERT(error == 0, "%s: error (%d) happens when getting reg",
		__func__, error);

	return error;
}

static void
vie_calc_bytereg(struct vie *vie, enum cpu_reg_name *reg, int *lhbr)
{
	*lhbr = 0;
	*reg = gpr_map[vie->reg];

	/*
	 * 64-bit mode imposes limitations on accessing legacy high byte
	 * registers (lhbr).
	 *
	 * The legacy high-byte registers cannot be addressed if the REX
	 * prefix is present. In this case the values 4, 5, 6 and 7 of the
	 * 'ModRM:reg' field address %spl, %bpl, %sil and %dil respectively.
	 *
	 * If the REX prefix is not present then the values 4, 5, 6 and 7
	 * of the 'ModRM:reg' field address the legacy high-byte registers,
	 * %ah, %ch, %dh and %bh respectively.
	 */
	if (vie->rex_present == 0U) {
		if ((vie->reg & 0x4U) != 0U) {
			*lhbr = 1;
			*reg = gpr_map[vie->reg & 0x3U];
		}
	}
}

static int
vie_read_bytereg(struct vcpu *vcpu, struct vie *vie, uint8_t *rval)
{
	uint64_t val;
	int error, lhbr;
	enum cpu_reg_name reg;

	vie_calc_bytereg(vie, &reg, &lhbr);
	error = vm_get_register(vcpu, reg, &val);

	/*
	 * To obtain the value of a legacy high byte register shift the
	 * base register right by 8 bits (%ah = %rax >> 8).
	 */
	if (lhbr != 0) {
		*rval = (uint8_t)(val >> 8);
	} else {
		*rval = (uint8_t)val;
	}
	return error;
}

static int
vie_write_bytereg(struct vcpu *vcpu, struct vie *vie, uint8_t byte)
{
	uint64_t origval, val, mask;
	int error, lhbr;
	enum cpu_reg_name reg;

	vie_calc_bytereg(vie, &reg, &lhbr);
	error = vm_get_register(vcpu, reg, &origval);
	if (error == 0) {
		val = byte;
		mask = 0xffU;
		if (lhbr != 0) {
			/*
			 * Shift left by 8 to store 'byte' in a legacy high
			 * byte register.
			 */
			val <<= 8;
			mask <<= 8;
		}
		val |= origval & ~mask;
		error = vm_set_register(vcpu, reg, val);
	}
	return error;
}

int
vie_update_register(struct vcpu *vcpu, enum cpu_reg_name reg,
		uint64_t val, uint8_t size)
{
	int error;
	uint64_t origval;

	switch (size) {
	case 1U:
	case 2U:
		error = vie_read_register(vcpu, reg, &origval);
		if (error != 0) {
			return error;
		}
		val &= size2mask[size];
		val |= origval & ~size2mask[size];
		break;
	case 4U:
		val &= 0xffffffffUL;
		break;
	case 8U:
		break;
	default:
		return -EINVAL;
	}

	error = vm_set_register(vcpu, reg, val);
	ASSERT(error == 0, "%s: Error (%d) happens when update reg",
		__func__, error);

	return error;
}

#define	RFLAGS_STATUS_BITS    (PSL_C | PSL_PF | PSL_AF | PSL_Z | PSL_N | PSL_V)

/*
 * Return the status flags that would result from doing (x - y).
 */
#define	GETCC(sz)							\
static uint64_t								\
getcc##sz(uint##sz##_t x, uint##sz##_t y)				\
{									\
	uint64_t rflags;						\
	\
	__asm __volatile("sub %2,%1; pushfq; popq %0" :			\
			"=r" (rflags), "+r" (x) : "m" (y));		\
	return rflags;						\
} struct __hack

GETCC(8);
GETCC(16);
GETCC(32);
GETCC(64);

static uint64_t
getcc(uint8_t opsize, uint64_t x, uint64_t y)
{
	ASSERT(opsize == 1U || opsize == 2U || opsize == 4U || opsize == 8U,
			"getcc: invalid operand size %hhu", opsize);

	if (opsize == 1U) {
		return getcc8((uint8_t)x, (uint8_t)y);
	} else if (opsize == 2U) {
		return getcc16((uint16_t)x, (uint16_t)y);
	} else if (opsize == 4U) {
		return getcc32((uint32_t)x, (uint32_t)y);
	} else {
		return getcc64(x, y);
	}
}

static int
emulate_mov(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint8_t byte;
	uint64_t val;

	size = vie->opsize;
	error = -EINVAL;
	switch (vie->op.op_byte) {
	case 0x88U:
	/*
	* MOV byte from reg (ModRM:reg) to mem (ModRM:r/m)
	* 88/r:	mov r/m8, r8
	* REX + 88/r:	mov r/m8, r8 (%ah, %ch, %dh, %bh not available)
	*/
		size = 1U;	/* override for byte operation */
		error = vie_read_bytereg(vcpu, vie, &byte);
		if (error == 0) {
			error = memwrite(vcpu, gpa, byte, size,
					arg);
		}
		break;
	case 0x89U:
		/*
		 * MOV from reg (ModRM:reg) to mem (ModRM:r/m)
		 * 89/r:	mov r/m16, r16
		 * 89/r:	mov r/m32, r32
		 * REX.W + 89/r	mov r/m64, r64
		 */

		reg = gpr_map[vie->reg];
		error = vie_read_register(vcpu, reg, &val);
		if (error == 0) {
			val &= size2mask[size];
			error = memwrite(vcpu, gpa, val, size,
					arg);
		}
		break;
	case 0x8AU:
		/*
		 * MOV byte from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8A/r:	mov r8, r/m8
		 * REX + 8A/r:	mov r8, r/m8
		 */
		size = 1U;	/* override for byte operation */
		error = memread(vcpu, gpa, &val, size, arg);
		if (error == 0) {
			error = vie_write_bytereg(vcpu, vie, (uint8_t)val);
		}
		break;
	case 0x8BU:
		/*
		 * MOV from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8B/r:	mov r16, r/m16
		 * 8B/r:	mov r32, r/m32
		 * REX.W 8B/r:	mov r64, r/m64
		 */
		error = memread(vcpu, gpa, &val, size, arg);
		if (error == 0) {
			reg = gpr_map[vie->reg];
			error = vie_update_register(vcpu, reg,
							val, size);
		}
		break;
	case 0xA1U:
		/*
		 * MOV from seg:moffset to AX/EAX/RAX
		 * A1:		mov AX, moffs16
		 * A1:		mov EAX, moffs32
		 * REX.W + A1:	mov RAX, moffs64
		 */
		error = memread(vcpu, gpa, &val, size, arg);
		if (error == 0) {
			reg = CPU_REG_RAX;
			error = vie_update_register(vcpu, reg,
							val, size);
		}
		break;
	case 0xA3U:
		/*
		 * MOV from AX/EAX/RAX to seg:moffset
		 * A3:		mov moffs16, AX
		 * A3:		mov moffs32, EAX
		 * REX.W + A3:	mov moffs64, RAX
		 */
		error = vie_read_register(vcpu, CPU_REG_RAX,
					&val);
		if (error == 0) {
			val &= size2mask[size];
			error = memwrite(vcpu, gpa, val, size,
					arg);
		}
		break;
	case 0xC6U:
		/*
		 * MOV from imm8 to mem (ModRM:r/m)
		 * C6/0		mov r/m8, imm8
		 * REX + C6/0	mov r/m8, imm8
		 */
		size = 1U;	/* override for byte operation */
		error = memwrite(vcpu, gpa, vie->immediate, size,
				arg);
		break;
	case 0xC7U:
		/*
		 * MOV from imm16/imm32 to mem (ModRM:r/m)
		 * C7/0		mov r/m16, imm16
		 * C7/0		mov r/m32, imm32
		 * REX.W + C7/0	mov r/m64, imm32
		 *		(sign-extended to 64-bits)
		 */
		val = (uint64_t)vie->immediate & size2mask[size];
		error = memwrite(vcpu, gpa, val, size, arg);
		break;
	default:
		break;
	}

	return error;
}

static int
emulate_movx(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, __unused mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t val;

	size = vie->opsize;
	error = -EINVAL;

	switch (vie->op.op_byte) {
	case 0xB6U:
		/*
		 * MOV and zero extend byte from mem (ModRM:r/m) to
		 * reg (ModRM:reg).
		 *
		 * 0F B6/r		movzx r16, r/m8
		 * 0F B6/r		movzx r32, r/m8
		 * REX.W + 0F B6/r	movzx r64, r/m8
		 */

		/* get the first operand */
		error = memread(vcpu, gpa, &val, 1U, arg);
		if (error != 0) {
			break;
		}

		/* get the second operand */
		reg = gpr_map[vie->reg];

		/* zero-extend byte */
		val = (uint8_t)val;

		/* write the result */
		error = vie_update_register(vcpu, reg, val, size);
		break;
	case 0xB7U:
		/*
		 * MOV and zero extend word from mem (ModRM:r/m) to
		 * reg (ModRM:reg).
		 *
		 * 0F B7/r		movzx r32, r/m16
		 * REX.W + 0F B7/r	movzx r64, r/m16
		 */
		error = memread(vcpu, gpa, &val, 2U, arg);
		if (error != 0) {
			return error;
		}

		reg = gpr_map[vie->reg];

		/* zero-extend word */
		val = (uint16_t)val;

		error = vie_update_register(vcpu, reg, val, size);
		break;
	case 0xBEU:
		/*
		 * MOV and sign extend byte from mem (ModRM:r/m) to
		 * reg (ModRM:reg).
		 *
		 * 0F BE/r		movsx r16, r/m8
		 * 0F BE/r		movsx r32, r/m8
		 * REX.W + 0F BE/r	movsx r64, r/m8
		 */

		/* get the first operand */
		error = memread(vcpu, gpa, &val, 1U, arg);
		if (error != 0) {
			break;
		}

		/* get the second operand */
		reg = gpr_map[vie->reg];

		/* sign extend byte */
		val = (int8_t)val;

		/* write the result */
		error = vie_update_register(vcpu, reg, val, size);
		break;
	default:
		break;
	}
	return error;
}

/*
 * Helper function to calculate and validate a linear address.
 */
static int
get_gla(struct vcpu *vcpu, __unused struct vie *vie,
	struct vm_guest_paging *paging,
	uint8_t opsize, uint8_t addrsize, uint32_t prot, enum cpu_reg_name seg,
	enum cpu_reg_name gpr, uint64_t *gla, int *fault)
{
	struct seg_desc desc;
	uint64_t cr0, val, rflags;
	int error;

	error = vie_read_register(vcpu, CPU_REG_CR0, &cr0);
	error |= vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);
	error |= vm_get_seg_desc(vcpu, seg, &desc);
	error |= vie_read_register(vcpu, gpr, &val);

	if (error) {
		pr_err("%s: error(%d) happens when getting cr0/rflags/segment"
			"desc/gpr", __func__, error);
		return -1;
	}

	if (vie_calculate_gla(paging->cpu_mode, seg, &desc, val, opsize,
	    addrsize, prot, gla) != 0) {
		if (seg == CPU_REG_SS) {
			/*vm_inject_ss(vcpu, 0);*/
			pr_err("TODO: inject ss exception");
		} else {
			/*vm_inject_gp(vcpu);*/
			pr_err("TODO: inject gp exception");
		}
		goto guest_fault;
	}

	if (vie_canonical_check(paging->cpu_mode, *gla) != 0) {
		if (seg == CPU_REG_SS) {
			/*vm_inject_ss(vcpu, 0);*/
			pr_err("TODO: inject ss exception");
		} else {
			/*vm_inject_gp(vcpu);*/
			pr_err("TODO: inject gp exception");
		}
		goto guest_fault;
	}

	if (vie_alignment_check(paging->cpl, opsize, cr0, rflags, *gla) != 0) {
		/*vm_inject_ac(vcpu, 0);*/
		pr_err("TODO: inject ac exception");
		goto guest_fault;
	}

	*fault = 0;
	return 0;

guest_fault:
	*fault = 1;
	return 0;
}

static int
emulate_movs(struct vcpu *vcpu, __unused uint64_t gpa, struct vie *vie,
		struct vm_guest_paging *paging,
		__unused mem_region_read_t memread,
		__unused mem_region_write_t memwrite,
		__unused void *arg)
{
	uint64_t dstaddr, srcaddr;
	uint64_t rcx, rdi, rsi, rflags;
	int error, fault, repeat;
	uint8_t opsize;
	enum cpu_reg_name seg;

	opsize = (vie->op.op_byte == 0xA4U) ? 1U : vie->opsize;
	error = 0;

	/*
	 * XXX although the MOVS instruction is only supposed to be used with
	 * the "rep" prefix some guests like FreeBSD will use "repnz" instead.
	 *
	 * Empirically the "repnz" prefix has identical behavior to "rep"
	 * and the zero flag does not make a difference.
	 */
	repeat = vie->repz_present | vie->repnz_present;

	if (repeat != 0) {
		error = vie_read_register(vcpu, CPU_REG_RCX, &rcx);

		/*
		 * The count register is %rcx, %ecx or %cx depending on the
		 * address size of the instruction.
		 */
		if ((rcx & vie_size2mask(vie->addrsize)) == 0UL) {
			error = 0;
			goto done;
		}
	}

	seg = (vie->segment_override != 0U) ? (vie->segment_register) : CPU_REG_DS;
	error = get_gla(vcpu, vie, paging, opsize, vie->addrsize,
	    PROT_READ, seg, CPU_REG_RSI, &srcaddr, &fault);
	if ((error != 0) || (fault != 0)) {
		goto done;
	}

	error = get_gla(vcpu, vie, paging, opsize, vie->addrsize,
		PROT_WRITE, CPU_REG_ES, CPU_REG_RDI, &dstaddr,
		&fault);
	if ((error != 0) || (fault != 0)) {
		goto done;
	}

	(void)memcpy_s((char *)dstaddr, 16U, (char *)srcaddr, opsize);

	error = vie_read_register(vcpu, CPU_REG_RSI, &rsi);
	error = vie_read_register(vcpu, CPU_REG_RDI, &rdi);
	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);

	if ((rflags & PSL_D) != 0U) {
		rsi -= opsize;
		rdi -= opsize;
	} else {
		rsi += opsize;
		rdi += opsize;
	}

	error = vie_update_register(vcpu, CPU_REG_RSI, rsi,
			vie->addrsize);

	error = vie_update_register(vcpu, CPU_REG_RDI, rdi,
			vie->addrsize);

	if (repeat != 0) {
		rcx = rcx - 1;
		error = vie_update_register(vcpu, CPU_REG_RCX,
				rcx, vie->addrsize);

		/*
		 * Repeat the instruction if the count register is not zero.
		 */
		if ((rcx & vie_size2mask(vie->addrsize)) != 0UL) {
			vcpu_retain_rip(vcpu);
		}
	}
done:
	ASSERT(error == 0, "%s: unexpected error %d", __func__, error);
	return error;
}

static int
emulate_stos(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		__unused struct vm_guest_paging *paging,
		__unused mem_region_read_t memread,
		mem_region_write_t memwrite, void *arg)
{
	int error, repeat;
	uint8_t opsize;
	uint64_t val;
	uint64_t rcx, rdi, rflags;

	opsize = (vie->op.op_byte == 0xAAU) ? 1U : vie->opsize;
	repeat = vie->repz_present | vie->repnz_present;

	if (repeat != 0) {
		error = vie_read_register(vcpu, CPU_REG_RCX, &rcx);

		/*
		 * The count register is %rcx, %ecx or %cx depending on the
		 * address size of the instruction.
		 */
		if ((rcx & vie_size2mask(vie->addrsize)) == 0UL) {
			return 0;
		}
	}

	error = vie_read_register(vcpu, CPU_REG_RAX, &val);

	error = memwrite(vcpu, gpa, val, opsize, arg);
	if (error != 0) {
		return error;
	}

	error = vie_read_register(vcpu, CPU_REG_RDI, &rdi);
	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);

	if ((rflags & PSL_D) != 0U) {
		rdi -= opsize;
	} else {
		rdi += opsize;
	}

	error = vie_update_register(vcpu, CPU_REG_RDI, rdi,
			vie->addrsize);

	if (repeat != 0) {
		rcx = rcx - 1;
		error = vie_update_register(vcpu, CPU_REG_RCX,
				rcx, vie->addrsize);

		/*
		 * Repeat the instruction if the count register is not zero.
		 */
		if ((rcx & vie_size2mask(vie->addrsize)) != 0UL) {
			vcpu_retain_rip(vcpu);
		}
	}

	return 0;
}

static int
emulate_test(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, __unused mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t result, rflags, rflags2, val1, val2;

	size = vie->opsize;
	error = -EINVAL;

	switch (vie->op.op_byte) {
	case 0x84U:
		/*
		 * 84/r		test r8, r/m8
		 */
		size = 1U; /*override size for 8-bit operation*/
		/* fallthrough */
	case 0x85U:
		/*
		 * AND reg (ModRM:reg) and mem (ModRM:r/m) and discard
		 * the result.
		 *
		 *
		 * 85/r		test r16, r/m16
		 * 85/r		test r32, r/m32
		 * REX.W + 85/r	test r64, r/m64
		 */

		/* get the first operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vcpu, reg, &val1);
		if (error != 0) {
			break;
		}

		/* get the second operand */
		error = memread(vcpu, gpa, &val2, size, arg);
		if (error != 0) {
			break;
		}

		/* perform the operation and write the result */
		result = val1 & val2;
		break;
	default:
		break;
	}
	if (error != 0) {
		return error;
	}

	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);
	if (error != 0) {
		return error;
	}

	/*
	 * OF and CF are cleared; the SF, ZF and PF flags are set according
	 * to the result; AF is undefined.
	 *
	 * The updated status flags are obtained by subtracting 0 from 'result'.
	 */
	rflags2 = getcc(size, result, 0UL);
	rflags &= ~RFLAGS_STATUS_BITS;
	rflags |= rflags2 & (PSL_PF | PSL_Z | PSL_N);
	size = 8U;
	error = vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);
	return error;
}

static int
emulate_and(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t result, rflags, rflags2, val1, val2;

	size = vie->opsize;
	error = -EINVAL;

	switch (vie->op.op_byte) {
	case 0x23U:
		/*
		 * AND reg (ModRM:reg) and mem (ModRM:r/m) and store the
		 * result in reg.
		 *
		 * 23/r		and r16, r/m16
		 * 23/r		and r32, r/m32
		 * REX.W + 23/r	and r64, r/m64
		 */

		/* get the first operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vcpu, reg, &val1);
		if (error != 0) {
			break;
		}

		/* get the second operand */
		error = memread(vcpu, gpa, &val2, size, arg);
		if (error != 0) {
			break;
		}

		/* perform the operation and write the result */
		result = val1 & val2;
		error = vie_update_register(vcpu, reg, result,
						size);
		break;
	case 0x81U:
	case 0x83U:
		/*
		 * AND mem (ModRM:r/m) with immediate and store the
		 * result in mem.
		 *
		 * 81 /4		and r/m16, imm16
		 * 81 /4		and r/m32, imm32
		 * REX.W + 81 /4	and r/m64, imm32 sign-extended to 64
		 *
		 * 83 /4		and r/m16, imm8 sign-extended to 16
		 * 83 /4		and r/m32, imm8 sign-extended to 32
		 * REX.W + 83/4		and r/m64, imm8 sign-extended to 64
		 */

		/* get the first operand */
		error = memread(vcpu, gpa, &val1, size, arg);
		if (error != 0) {
			break;
		}

		/*
		 * perform the operation with the pre-fetched immediate
		 * operand and write the result
		 */
		result = val1 & vie->immediate;
		error = memwrite(vcpu, gpa, result, size, arg);
		break;
	default:
		break;
	}
	if (error != 0) {
		return error;
	}

	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);
	if (error != 0) {
		return error;
	}

	/*
	 * OF and CF are cleared; the SF, ZF and PF flags are set according
	 * to the result; AF is undefined.
	 *
	 * The updated status flags are obtained by subtracting 0 from 'result'.
	 */
	rflags2 = getcc(size, result, 0UL);
	rflags &= ~RFLAGS_STATUS_BITS;
	rflags |= rflags2 & (PSL_PF | PSL_Z | PSL_N);
	size = 8U;
	error = vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);
	return error;
}

static int
emulate_or(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t val1, val2, result, rflags, rflags2;

	size = vie->opsize;
	error = -EINVAL;

	switch (vie->op.op_byte) {
	case 0x81U:
	case 0x83U:
		/*
		 * OR mem (ModRM:r/m) with immediate and store the
		 * result in mem.
		 *
		 * 81 /1		or r/m16, imm16
		 * 81 /1		or r/m32, imm32
		 * REX.W + 81 /1	or r/m64, imm32 sign-extended to
		 *			64
		 *
		 * 83 /1		or r/m16, imm8 sign-extended to
		 *			16
		 * 83 /1		or r/m32, imm8 sign-extended to
		 *			32
		 * REX.W + 83/1		or r/m64, imm8 sign-extended to
		 *			64
		 */

		/* get the first operand */
		error = memread(vcpu, gpa, &val1, size, arg);
		if (error != 0) {
			break;
		}

		/*
		 * perform the operation with the pre-fetched immediate
		 * operand and write the result
		 */
		result = val1 | (uint64_t)vie->immediate;
		error = memwrite(vcpu, gpa, result, size, arg);
		break;
	case 0x09U:
		/*
		 * OR mem (ModRM:r/m) with reg (ModRM:reg) and store the
		 * result in mem.
		 * 09/r:    OR r/m16, r16
		 * 09/r:    OR r/m32, r32
		 */

		/* get the first operand */
		error = memread(vcpu, gpa, &val1, size, arg);
		if (error != 0) {
			break;
		}

		/* get the second operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vcpu, reg, &val2);
		if (error != 0) {
			break;
		}

		/* perform the operation and write the result */
		result = val1 | val2;
		result &= size2mask[size];

		error = memwrite(vcpu, gpa, result, size, arg);
		break;
	default:
		break;
	}
	if (error != 0) {
		return error;
	}

	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);
	if (error != 0) {
		return error;
	}

	/*
	 * OF and CF are cleared; the SF, ZF and PF flags are set according
	 * to the result; AF is undefined.
	 *
	 * The updated status flags are obtained by subtracting 0 from 'result'.
	 */
	rflags2 = getcc(size, result, 0UL);
	rflags &= ~RFLAGS_STATUS_BITS;
	rflags |= rflags2 & (PSL_PF | PSL_Z | PSL_N);
	size = 8U;
	error = vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);
	return error;
}

static int
emulate_cmp(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, __unused mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	uint64_t regop, memop, op1, op2, rflags, rflags2;
	enum cpu_reg_name reg;

	size = vie->opsize;
	switch (vie->op.op_byte) {
	case 0x39U:
	case 0x3BU:
		/*
		 * 39/r		CMP r/m16, r16
		 * 39/r		CMP r/m32, r32
		 * REX.W 39/r	CMP r/m64, r64
		 *
		 * 3B/r		CMP r16, r/m16
		 * 3B/r		CMP r32, r/m32
		 * REX.W + 3B/r	CMP r64, r/m64
		 *
		 * Compare the first operand with the second operand and
		 * set status flags in EFLAGS register. The comparison
		 * is performed by subtracting the second operand from
		 * the first operand and then setting the status flags.
		 */

		/* Get the register operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vcpu, reg, &regop);
		if (error != 0) {
			return error;
		}

		/* Get the memory operand */
		error = memread(vcpu, gpa, &memop, size, arg);
		if (error != 0) {
			return error;
		}

		if (vie->op.op_byte == 0x3BU) {
			op1 = regop;
			op2 = memop;
		} else {
			op1 = memop;
			op2 = regop;
		}
		rflags2 = getcc(size, op1, op2);
		break;
	case 0x80U:
	case 0x81U:
	case 0x83U:
		/*
		 * 80 /7		cmp r/m8, imm8
		 * REX + 80 /7		cmp r/m8, imm8
		 *
		 * 81 /7		cmp r/m16, imm16
		 * 81 /7		cmp r/m32, imm32
		 * REX.W + 81 /7	cmp r/m64, imm32 sign-extended
		 *			to 64
		 *
		 * 83 /7		cmp r/m16, imm8 sign-extended
		 *			to 16
		 * 83 /7		cmp r/m32, imm8 sign-extended
		 *			to 32
		 * REX.W + 83 /7	cmp r/m64, imm8 sign-extended
		 *			to 64
		 *
		 * Compare mem (ModRM:r/m) with immediate and set
		 * status flags according to the results.  The
		 * comparison is performed by subtracting the
		 * immediate from the first operand and then setting
		 * the status flags.
		 *
		 */
		if (vie->op.op_byte == 0x80U) {
			size = 1U;
		}

		/* get the first operand */
		error = memread(vcpu, gpa, &op1, size, arg);
		if (error != 0) {
			return error;
		}

		rflags2 = getcc(size, op1, (uint64_t)vie->immediate);
		break;
	default:
		return -EINVAL;
	}
	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);
	if (error != 0) {
		return error;
	}
	rflags &= ~RFLAGS_STATUS_BITS;
	rflags |= rflags2 & RFLAGS_STATUS_BITS;
	size = 8U;
	error = vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);
	return error;
}

static int
emulate_sub(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, __unused mem_region_write_t memwrite,
		void *arg)
{
	int error;
	uint8_t size;
	uint64_t nval, rflags, rflags2, val1, val2;
	enum cpu_reg_name reg;

	size = vie->opsize;
	error = -EINVAL;

	switch (vie->op.op_byte) {
	case 0x2BU:
		/*
		 * SUB r/m from r and store the result in r
		 *
		 * 2B/r            SUB r16, r/m16
		 * 2B/r            SUB r32, r/m32
		 * REX.W + 2B/r    SUB r64, r/m64
		 */

		/* get the first operand */
		reg = gpr_map[vie->reg];
		error = vie_read_register(vcpu, reg, &val1);
		if (error != 0) {
			break;
		}

		/* get the second operand */
		error = memread(vcpu, gpa, &val2, size, arg);
		if (error != 0) {
			break;
		}

		/* perform the operation and write the result */
		nval = val1 - val2;
		error = vie_update_register(vcpu, reg, nval,
						size);
		break;
	default:
		break;
	}

	if (error == 0) {
		rflags2 = getcc(size, val1, val2);
		error = vie_read_register(vcpu, CPU_REG_RFLAGS,
				&rflags);
		if (error != 0) {
			return error;
		}

		rflags &= ~RFLAGS_STATUS_BITS;
		rflags |= rflags2 & RFLAGS_STATUS_BITS;
		size = 8U;
		error = vie_update_register(vcpu, CPU_REG_RFLAGS,
				rflags, size);
	}

	return error;
}

static int
emulate_stack_op(struct vcpu *vcpu, uint64_t mmio_gpa, struct vie *vie,
		struct vm_guest_paging *paging, mem_region_read_t memread,
		mem_region_write_t memwrite, void *arg)
{
	struct seg_desc ss_desc;
	uint64_t cr0, rflags, rsp, stack_gla, stack_gpa, val;
	int error, pushop;
	uint8_t size, stackaddrsize;
	uint32_t err_code = 0U;

	(void)memset(&ss_desc, 0U, sizeof(ss_desc));

	val = 0UL;
	size = vie->opsize;
	pushop = (vie->op.op_type == VIE_OP_TYPE_PUSH) ? 1 : 0;

	/*
	 * From "Address-Size Attributes for Stack Accesses", Intel SDL, Vol 1
	 */
	if (paging->cpu_mode == CPU_MODE_REAL) {
		stackaddrsize = 2U;
	} else if (paging->cpu_mode == CPU_MODE_64BIT) {
		/*
		 * "Stack Manipulation Instructions in 64-bit Mode", SDM, Vol 3
		 * - Stack pointer size is always 64-bits.
		 * - PUSH/POP of 32-bit values is not possible in 64-bit mode.
		 * - 16-bit PUSH/POP is supported by using the operand size
		 *   override prefix (66H).
		 */
		stackaddrsize = 8U;
		size = (vie->opsize_override != 0U) ? 2U : 8U;
	} else {
		/*
		 * In protected or compatibility mode the 'B' flag in the
		 * stack-segment descriptor determines the size of the
		 * stack pointer.
		 */
		error = vm_get_seg_desc(vcpu, CPU_REG_SS, &ss_desc);
		ASSERT(error == 0, "%s: error %d getting SS descriptor",
					__func__, error);
		if (SEG_DESC_DEF32(ss_desc.access)) {
			stackaddrsize = 4U;
		} else {
			stackaddrsize = 2U;
		}
	}

	error = vie_read_register(vcpu, CPU_REG_CR0, &cr0);
	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);
	error = vie_read_register(vcpu, CPU_REG_RSP, &rsp);

	if (pushop != 0) {
		rsp -= size;
	}

	if (vie_calculate_gla(paging->cpu_mode, CPU_REG_SS, &ss_desc,
	    rsp, size, stackaddrsize, (pushop != 0)? PROT_WRITE : PROT_READ,
	    &stack_gla) != 0) {
		/*vm_inject_ss(vcpu, 0);*/
		pr_err("TODO: inject ss exception");
	}

	if (vie_canonical_check(paging->cpu_mode, stack_gla) != 0) {
		/*vm_inject_ss(vcpu, 0);*/
		pr_err("TODO: inject ss exception");
	}

	if (vie_alignment_check(paging->cpl, size, cr0, rflags, stack_gla) != 0) {
		/*vm_inject_ac(vcpu, 0);*/
		pr_err("TODO: inject ac exception");
		return 0;
	}

	/* TODO: currently emulate_instruction is only for mmio, so here
	 * stack_gpa actually is unused for mmio_write & mmio_read, need
	 * take care of data trans if stack_gpa be used for memwrite in
	 * the future.
	 */
	if (pushop != 0) {
		err_code |= PAGE_FAULT_WR_FLAG;
	}
	error = gva2gpa(vcpu, stack_gla, &stack_gpa, &err_code);
	if (error == -EFAULT) {
		vcpu_inject_pf(vcpu, stack_gla, err_code);
		return error;
	} else if (error < 0) {
		return error;
	}
	if (pushop != 0) {
		error = memread(vcpu, mmio_gpa, &val, size, arg);
		if (error == 0) {
			error = memwrite(vcpu, stack_gpa, val, size, arg);
		}
	} else {
		error = memread(vcpu, stack_gpa, &val, size, arg);
		if (error == 0) {
			error = memwrite(vcpu, mmio_gpa, val, size, arg);
		}
		rsp += size;
	}


	if (error == 0) {
		error = vie_update_register(vcpu, CPU_REG_RSP, rsp,
				stackaddrsize);
	}
	return error;
}

static int
emulate_push(struct vcpu *vcpu, uint64_t mmio_gpa, struct vie *vie,
		struct vm_guest_paging *paging, mem_region_read_t memread,
		mem_region_write_t memwrite, void *arg)
{
	int error;

	/*
	 * Table A-6, "Opcode Extensions", Intel SDM, Vol 2.
	 *
	 * PUSH is part of the group 5 extended opcodes and is identified
	 * by ModRM:reg = b110.
	 */
	if ((vie->reg & 7U) != 6U) {
		return -EINVAL;
	}

	error = emulate_stack_op(vcpu, mmio_gpa, vie, paging, memread,
			memwrite, arg);
	return error;
}

static int
emulate_pop(struct vcpu *vcpu, uint64_t mmio_gpa, struct vie *vie,
		struct vm_guest_paging *paging, mem_region_read_t memread,
		mem_region_write_t memwrite, void *arg)
{
	int error;

	/*
	 * Table A-6, "Opcode Extensions", Intel SDM, Vol 2.
	 *
	 * POP is part of the group 1A extended opcodes and is identified
	 * by ModRM:reg = b000.
	 */
	if ((vie->reg & 7U) != 0) {
		return -EINVAL;
	}

	error = emulate_stack_op(vcpu, mmio_gpa, vie, paging, memread,
			memwrite, arg);
	return error;
}

static int
emulate_group1(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		__unused struct vm_guest_paging *paging,
		mem_region_read_t memread,
		mem_region_write_t memwrite, void *memarg)
{
	int error;

	switch (vie->reg & 7U) {
	case 0x1U:	/* OR */
		error = emulate_or(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case 0x4U:	/* AND */
		error = emulate_and(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case 0x7U:	/* CMP */
		error = emulate_cmp(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int
emulate_bittest(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		mem_region_read_t memread, __unused mem_region_write_t memwrite,
		void *memarg)
{
	uint64_t val, rflags, bitmask;
	int error;
	uint32_t  bitoff;
	uint8_t size;

	/*
	 * 0F BA is a Group 8 extended opcode.
	 *
	 * Currently we only emulate the 'Bit Test' instruction which is
	 * identified by a ModR/M:reg encoding of 100b.
	 */
	if ((vie->reg & 7U) != 4U) {
		return -EINVAL;
	}

	error = vie_read_register(vcpu, CPU_REG_RFLAGS, &rflags);

	error = memread(vcpu, gpa, &val, vie->opsize, memarg);
	if (error != 0) {
		return error;
	}

	/*
	 * Intel SDM, Vol 2, Table 3-2:
	 * "Range of Bit Positions Specified by Bit Offset Operands"
	 */
	bitmask = (uint64_t)vie->opsize * 8UL - 1UL;
	bitoff = (uint64_t)vie->immediate & bitmask;

	/* Copy the bit into the Carry flag in %rflags */
	if ((val & (1UL << bitoff)) != 0U) {
		rflags |= PSL_C;
	} else {
		rflags &= ~PSL_C;
	}
	size = 8U;
	error = vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);

	return 0;
}

int
vmm_emulate_instruction(struct vcpu *vcpu, uint64_t gpa, struct vie *vie,
		struct vm_guest_paging *paging, mem_region_read_t memread,
		mem_region_write_t memwrite, void *memarg)
{
	int error;

	if (vie->decoded == 0U) {
		return -EINVAL;
	}
	switch (vie->op.op_type) {
	case VIE_OP_TYPE_GROUP1:
		error = emulate_group1(vcpu, gpa, vie, paging,
					memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_POP:
		error = emulate_pop(vcpu, gpa, vie, paging,
					memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_PUSH:
		error = emulate_push(vcpu, gpa, vie, paging,
					memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_CMP:
		error = emulate_cmp(vcpu, gpa, vie,
					memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_MOV:
		error = emulate_mov(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_MOVSX:
	case VIE_OP_TYPE_MOVZX:
		error = emulate_movx(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_MOVS:
		error = emulate_movs(vcpu, gpa, vie, paging,
					memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_STOS:
		error = emulate_stos(vcpu, gpa, vie, paging,
					memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_AND:
		error = emulate_and(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_TEST:
		error = emulate_test(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_OR:
		error = emulate_or(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_SUB:
		error = emulate_sub(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	case VIE_OP_TYPE_BITTEST:
		error = emulate_bittest(vcpu, gpa, vie,
				memread, memwrite, memarg);
		break;
	default:
		error = -EINVAL;
		break;
	}
	return error;
}

int
vie_alignment_check(uint8_t cpl, uint8_t size, uint64_t cr0, uint64_t rf, uint64_t gla)
{
	ASSERT(size == 1U || size == 2U || size == 4U || size == 8U,
	    "%s: invalid size %hhu", __func__, size);
	ASSERT(cpl <= 3U, "%s: invalid cpl %d", __func__, cpl);

	if (cpl != 3U || (cr0 & CR0_AM) == 0UL || (rf & PSL_AC) == 0UL) {
		return 0;
	}

	return ((gla & (size - 1U)) != 0UL) ? 1 : 0;
}

int
vie_canonical_check(enum vm_cpu_mode cpu_mode, uint64_t gla)
{
	uint64_t mask;

	if (cpu_mode != CPU_MODE_64BIT) {
		return 0;
	}

	/*
	 * The value of the bit 47 in the 'gla' should be replicated in the
	 * most significant 16 bits.
	 */
	mask = ~((1UL << 48) - 1);
	if ((gla & (1UL << 47)) != 0U) {
		return ((gla & mask) != mask) ? 1 : 0;
	} else {
		return ((gla & mask) != 0U) ? 1 : 0;
	}
}

uint64_t
vie_size2mask(uint8_t size)
{
	ASSERT(size == 1U || size == 2U || size == 4U || size == 8U,
			"vie_size2mask: invalid size %hhu", size);
	return size2mask[size];
}

int
vie_calculate_gla(enum vm_cpu_mode cpu_mode, enum cpu_reg_name seg,
	struct seg_desc *desc, uint64_t offset, uint8_t length, uint8_t addrsize,
	uint32_t prot, uint64_t *gla)
{
	uint64_t firstoff, low_limit, high_limit, segbase;
	uint8_t glasize;
	uint32_t type;

	ASSERT(seg >= CPU_REG_ES && seg <= CPU_REG_GS,
	    "%s: invalid segment %d", __func__, seg);
	ASSERT(length == 1U || length == 2U || length == 4U || length == 8U,
	    "%s: invalid operand size %hhu", __func__, length);
	ASSERT((prot & ~(PROT_READ | PROT_WRITE)) == 0,
	    "%s: invalid prot %#x", __func__, prot);

	firstoff = offset;
	if (cpu_mode == CPU_MODE_64BIT) {
		ASSERT(addrsize == 4U || addrsize == 8U,
			"%s: invalid address size %d for cpu_mode %d",
			__func__, addrsize, cpu_mode);
		glasize = 8U;
	} else {
		ASSERT(addrsize == 2U || addrsize == 4U,
			"%s: invalid address size %d for cpu mode %d",
			__func__, addrsize, cpu_mode);
		glasize = 4U;
		/*
		 * If the segment selector is loaded with a NULL selector
		 * then the descriptor is unusable and attempting to use
		 * it results in a #GP(0).
		 */
		if (SEG_DESC_UNUSABLE(desc->access)) {
			return -1;
		}

		/*
		 * The processor generates a #NP exception when a segment
		 * register is loaded with a selector that points to a
		 * descriptor that is not present. If this was the case then
		 * it would have been checked before the VM-exit.
		 */
		ASSERT(SEG_DESC_PRESENT(desc->access),
		    "segment %d not present: %#x", seg, desc->access);

		/*
		 * The descriptor type must indicate a code/data segment.
		 */
		type = SEG_DESC_TYPE(desc->access);
		ASSERT(type >= 16U && type <= 31U,
			"segment %d has invalid descriptor type %#x",
			seg, type);

		if ((prot & PROT_READ) != 0U) {
			/* #GP on a read access to a exec-only code segment */
			if ((type & 0xAU) == 0x8U) {
				return -1;
			}
		}

		if ((prot & PROT_WRITE) != 0U) {
			/*
			 * #GP on a write access to a code segment or a
			 * read-only data segment.
			 */
			if ((type & 0x8U) != 0U) {		/* code segment */
				return -1;
			}

			if ((type & 0xAU) == 0U) {		/* read-only data seg */
				return -1;
			}
		}

		/*
		 * 'desc->limit' is fully expanded taking granularity into
		 * account.
		 */
		if ((type & 0xCU) == 0x4U) {
			/* expand-down data segment */
			low_limit = desc->limit + 1U;
			high_limit = SEG_DESC_DEF32(desc->access) ?
			    0xffffffffU : 0xffffU;
		} else {
			/* code segment or expand-up data segment */
			low_limit = 0U;
			high_limit = desc->limit;
		}

		while (length > 0U) {
			offset &= vie_size2mask(addrsize);
			if (offset < low_limit || offset > high_limit) {
				return -1;
			}
			offset++;
			length--;
		}
	}

	/*
	 * In 64-bit mode all segments except %fs and %gs have a segment
	 * base address of 0.
	 */
	if (cpu_mode == CPU_MODE_64BIT && seg != CPU_REG_FS &&
	    seg != CPU_REG_GS) {
		segbase = 0UL;
	} else {
		segbase = desc->base;
	}

	/*
	 * Truncate 'firstoff' to the effective address size before adding
	 * it to the segment base.
	 */
	firstoff &= vie_size2mask(addrsize);
	*gla = (segbase + firstoff) & vie_size2mask(glasize);
	return 0;
}

int
vie_init(struct vie *vie, struct vcpu *vcpu)
{
	uint64_t guest_rip_gva =
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].rip;
	uint32_t inst_len = vcpu->arch_vcpu.inst_len;
	uint32_t err_code;
	int ret;

	if (inst_len > VIE_INST_SIZE || inst_len == 0U) {
		pr_err("%s: invalid instruction length (%d)",
			__func__, inst_len);
		return -EINVAL;
	}

	(void)memset(vie, 0U, sizeof(struct vie));

	vie->base_register = CPU_REG_LAST;
	vie->index_register = CPU_REG_LAST;
	vie->segment_register = CPU_REG_LAST;

	err_code = PAGE_FAULT_ID_FLAG;
	ret = copy_from_gva(vcpu, vie->inst, guest_rip_gva,
				inst_len, &err_code);
	if (ret == -EFAULT) {
		vcpu_inject_pf(vcpu, guest_rip_gva, err_code);
		return ret;
	} else if (ret < 0) {
		return ret;
	}

	vie->num_valid = (uint8_t)inst_len;

	return 0;
}

static int
vie_peek(struct vie *vie, uint8_t *x)
{

	if (vie->num_processed < vie->num_valid) {
		*x = vie->inst[vie->num_processed];
		return 0;
	} else {
		return -1;
	}
}

static void
vie_advance(struct vie *vie)
{

	vie->num_processed++;
}

static bool
segment_override(uint8_t x, enum cpu_reg_name *seg)
{

	switch (x) {
	case 0x2EU:
		*seg = CPU_REG_CS;
		break;
	case 0x36U:
		*seg = CPU_REG_SS;
		break;
	case 0x3EU:
		*seg = CPU_REG_DS;
		break;
	case 0x26U:
		*seg = CPU_REG_ES;
		break;
	case 0x64U:
		*seg = CPU_REG_FS;
		break;
	case 0x65U:
		*seg = CPU_REG_GS;
		break;
	default:
		return false;
	}
	return true;
}

static int
decode_prefixes(struct vie *vie, enum vm_cpu_mode cpu_mode, bool cs_d)
{
	uint8_t x;

	while (1) {
		if (vie_peek(vie, &x) != 0) {
			return -1;
		}

		if (x == 0x66U) {
			vie->opsize_override = 1U;
		} else if (x == 0x67U) {
			vie->addrsize_override = 1U;
		} else if (x == 0xF3U) {
			vie->repz_present = 1U;
		} else if (x == 0xF2U) {
			vie->repnz_present = 1U;
		} else if (segment_override(x, &vie->segment_register)) {
			vie->segment_override = 1U;
		} else {
			break;
		}

		vie_advance(vie);
	}

	/*
	 * From section 2.2.1, "REX Prefixes", Intel SDM Vol 2:
	 * - Only one REX prefix is allowed per instruction.
	 * - The REX prefix must immediately precede the opcode byte or the
	 *   escape opcode byte.
	 * - If an instruction has a mandatory prefix (0x66, 0xF2 or 0xF3)
	 *   the mandatory prefix must come before the REX prefix.
	 */
	if (cpu_mode == CPU_MODE_64BIT && x >= 0x40U && x <= 0x4FU) {
		vie->rex_present = 1U;
		vie->rex_w = (x & 0x8U) != 0U ? 1U : 0U;
		vie->rex_r = (x & 0x4U) != 0U ? 1U : 0U;
		vie->rex_x = (x & 0x2U) != 0U ? 1U : 0U;
		vie->rex_b = (x & 0x1U) != 0U ? 1U : 0U;
		vie_advance(vie);
	}

	/*
	 * Section "Operand-Size And Address-Size Attributes", Intel SDM, Vol 1
	 */
	if (cpu_mode == CPU_MODE_64BIT) {
		/*
		 * Default address size is 64-bits and default operand size
		 * is 32-bits.
		 */
		vie->addrsize = (vie->addrsize_override != 0U)? 4U : 8U;
		if (vie->rex_w != 0U) {
			vie->opsize = 8U;
		} else if (vie->opsize_override != 0U) {
			vie->opsize = 2U;
		} else {
			vie->opsize = 4U;
		}
	} else if (cs_d) {
		/* Default address and operand sizes are 32-bits */
		vie->addrsize = vie->addrsize_override != 0U ? 2U : 4U;
		vie->opsize = vie->opsize_override != 0U ? 2U : 4U;
	} else {
		/* Default address and operand sizes are 16-bits */
		vie->addrsize = vie->addrsize_override != 0U ? 4U : 2U;
		vie->opsize = vie->opsize_override != 0U ? 4U : 2U;
	}
	return 0;
}

static int
decode_two_byte_opcode(struct vie *vie)
{
	uint8_t x;

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	vie->op = two_byte_opcodes[x];

	if (vie->op.op_type == VIE_OP_TYPE_NONE) {
		return -1;
	}

	vie_advance(vie);
	return 0;
}

static int
decode_opcode(struct vie *vie)
{
	uint8_t x;

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	vie->op = one_byte_opcodes[x];

	if (vie->op.op_type == VIE_OP_TYPE_NONE) {
		return -1;
	}

	vie_advance(vie);

	if (vie->op.op_type == VIE_OP_TYPE_TWO_BYTE) {
		return decode_two_byte_opcode(vie);
	}

	return 0;
}

static int
decode_modrm(struct vie *vie, enum vm_cpu_mode cpu_mode)
{
	uint8_t x;

	if ((vie->op.op_flags & VIE_OP_F_NO_MODRM) != 0U) {
		return 0;
	}

	if (cpu_mode == CPU_MODE_REAL) {
		return -1;
	}

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	vie->mod = (x >> 6) & 0x3U;
	vie->rm =  (x >> 0) & 0x7U;
	vie->reg = (x >> 3) & 0x7U;

	/*
	 * A direct addressing mode makes no sense in the context of an EPT
	 * fault. There has to be a memory access involved to cause the
	 * EPT fault.
	 */
	if (vie->mod == VIE_MOD_DIRECT) {
		return -1;
	}

	if ((vie->mod == VIE_MOD_INDIRECT && vie->rm == VIE_RM_DISP32) ||
			(vie->mod != VIE_MOD_DIRECT && vie->rm == VIE_RM_SIB)) {
		/*
		 * Table 2-5: Special Cases of REX Encodings
		 *
		 * mod=0, r/m=5 is used in the compatibility mode to
		 * indicate a disp32 without a base register.
		 *
		 * mod!=3, r/m=4 is used in the compatibility mode to
		 * indicate that the SIB byte is present.
		 *
		 * The 'b' bit in the REX prefix is don't care in
		 * this case.
		 */
	} else {
		vie->rm |= (vie->rex_b << 3);
	}

	vie->reg |= (vie->rex_r << 3);

	vie_advance(vie);

	return 0;
}

static int
decode_sib(struct vie *vie)
{
	uint8_t x;

	/* Proceed only if SIB byte is present */
	if (vie->mod == VIE_MOD_DIRECT || vie->rm != VIE_RM_SIB) {
		return 0;
	}

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	/* De-construct the SIB byte */
	vie->ss = (x >> 6) & 0x3U;
	vie->index = (x >> 3) & 0x7U;
	vie->base = (x >> 0) & 0x7U;

	/* Apply the REX prefix modifiers */
	vie->index |= vie->rex_x << 3;
	vie->base |= vie->rex_b << 3;

	switch (vie->mod) {
	case VIE_MOD_INDIRECT_DISP8:
		vie->disp_bytes = 1U;
		break;
	case VIE_MOD_INDIRECT_DISP32:
		vie->disp_bytes = 4U;
		break;
	}

	if (vie->mod == VIE_MOD_INDIRECT &&
			(vie->base == 5U || vie->base == 13U)) {
		/*
		 * Special case when base register is unused if mod = 0
		 * and base = %rbp or %r13.
		 *
		 * Documented in:
		 * Table 2-3: 32-bit Addressing Forms with the SIB Byte
		 * Table 2-5: Special Cases of REX Encodings
		 */
		vie->disp_bytes = 4U;
	} else {
		vie->base_register = gpr_map[vie->base];
	}

	/*
	 * All encodings of 'index' are valid except for %rsp (4).
	 *
	 * Documented in:
	 * Table 2-3: 32-bit Addressing Forms with the SIB Byte
	 * Table 2-5: Special Cases of REX Encodings
	 */
	if (vie->index != 4U) {
		vie->index_register = gpr_map[vie->index];
	}

	/* 'scale' makes sense only in the context of an index register */
	if (vie->index_register < CPU_REG_LAST) {
		vie->scale = 1U << vie->ss;
	}

	vie_advance(vie);

	return 0;
}

static int
decode_displacement(struct vie *vie)
{
	int n, i;
	uint8_t x;

	union {
		uint8_t	buf[4];
		int8_t	signed8;
		int32_t	signed32;
	} u;

	n = vie->disp_bytes;
	if (n == 0) {
		return 0;
	}

	if (n != 1 && n != 4) {
		panic("decode_displacement: invalid disp_bytes %d", n);
	}

	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x) != 0) {
			return -1;
		}

		u.buf[i] = x;
		vie_advance(vie);
	}

	if (n == 1) {
		vie->displacement = u.signed8;		/* sign-extended */
	} else {
		vie->displacement = u.signed32;		/* sign-extended */
	}

	return 0;
}

static int
decode_immediate(struct vie *vie)
{
	int i, n;
	uint8_t x;
	union {
		uint8_t	buf[4];
		int8_t	signed8;
		int16_t	signed16;
		int32_t	signed32;
	} u;

	/* Figure out immediate operand size (if any) */
	if ((vie->op.op_flags & VIE_OP_F_IMM) != 0U) {
		/*
		 * Section 2.2.1.5 "Immediates", Intel SDM:
		 * In 64-bit mode the typical size of immediate operands
		 * remains 32-bits. When the operand size if 64-bits, the
		 * processor sign-extends all immediates to 64-bits prior
		 * to their use.
		 */
		if (vie->opsize == 4U || vie->opsize == 8U) {
			vie->imm_bytes = 4U;
		}
		else {
			vie->imm_bytes = 2U;
		}
	} else if ((vie->op.op_flags & VIE_OP_F_IMM8) != 0U) {
		vie->imm_bytes = 1U;
	}

	n = vie->imm_bytes;
	if (n == 0) {
		return 0;
	}

	ASSERT(n == 1 || n == 2 || n == 4,
		"%s: invalid number of immediate bytes: %d", __func__, n);

	for (i = 0; i < n; i++) {
		if (vie_peek(vie, &x) != 0) {
			return -1;
		}

		u.buf[i] = x;
		vie_advance(vie);
	}

	/* sign-extend the immediate value before use */
	if (n == 1) {
		vie->immediate = u.signed8;
	} else if (n == 2) {
		vie->immediate = u.signed16;
	} else {
		vie->immediate = u.signed32;
	}

	return 0;
}

static int
decode_moffset(struct vie *vie)
{
	uint8_t i, n, x;
	union {
		uint8_t  buf[8];
		uint64_t u64;
	} u;

	if ((vie->op.op_flags & VIE_OP_F_MOFFSET) == 0U) {
		return 0;
	}

	/*
	 * Section 2.2.1.4, "Direct Memory-Offset MOVs", Intel SDM:
	 * The memory offset size follows the address-size of the instruction.
	 */
	n = vie->addrsize;
	ASSERT(n == 2U || n == 4U || n == 8U, "invalid moffset bytes: %hhu", n);

	u.u64 = 0UL;
	for (i = 0U; i < n; i++) {
		if (vie_peek(vie, &x) != 0) {
			return -1;
		}

		u.buf[i] = x;
		vie_advance(vie);
	}
	vie->displacement = (int64_t)u.u64;
	return 0;
}

int
__decode_instruction(enum vm_cpu_mode cpu_mode, bool cs_d, struct vie *vie)
{
	if (decode_prefixes(vie, cpu_mode, cs_d) != 0) {
		return -1;
	}

	if (decode_opcode(vie) != 0) {
		return -1;
	}

	if (decode_modrm(vie, cpu_mode) != 0) {
		return -1;
	}

	if (decode_sib(vie) != 0) {
		return -1;
	}

	if (decode_displacement(vie) != 0) {
		return -1;
	}

	if (decode_immediate(vie) != 0) {
		return -1;
	}

	if (decode_moffset(vie) != 0) {
		return -1;
	}

	vie->decoded = 1U;	/* success */

	return 0;
}
