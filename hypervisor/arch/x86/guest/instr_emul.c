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
#define	VIE_OP_F_IMM		(1U << 0U)  /* 16/32-bit immediate operand */
#define	VIE_OP_F_IMM8		(1U << 1U)  /* 8-bit immediate operand */
#define	VIE_OP_F_MOFFSET	(1U << 2U)  /* 16/32/64-bit immediate moffset */
#define	VIE_OP_F_NO_MODRM	(1U << 3U)
#define	VIE_OP_F_CHECK_GVA_DI   (1U << 4U)  /* for movs, need to check DI */

static const struct instr_emul_vie_op two_byte_opcodes[256] = {
	[0xB6] = {
		.op_type = VIE_OP_TYPE_MOVZX,
	},
	[0xB7] = {
		.op_type = VIE_OP_TYPE_MOVZX,
	},
	[0xBA] = {
		.op_type = VIE_OP_TYPE_BITTEST,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0xBE] = {
		.op_type = VIE_OP_TYPE_MOVSX,
	},
};

static const struct instr_emul_vie_op one_byte_opcodes[256] = {
	[0x0F] = {
		.op_type = VIE_OP_TYPE_TWO_BYTE
	},
	[0x2B] = {
		.op_type = VIE_OP_TYPE_SUB,
	},
	[0x39] = {
		.op_type = VIE_OP_TYPE_CMP,
	},
	[0x3B] = {
		.op_type = VIE_OP_TYPE_CMP,
	},
	[0x88] = {
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x89] = {
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8A] = {
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0x8B] = {
		.op_type = VIE_OP_TYPE_MOV,
	},
	[0xA1] = {
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_MOFFSET | VIE_OP_F_NO_MODRM,
	},
	[0xA3] = {
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_MOFFSET | VIE_OP_F_NO_MODRM,
	},
	[0xA4] = {
		.op_type = VIE_OP_TYPE_MOVS,
		.op_flags = VIE_OP_F_NO_MODRM | VIE_OP_F_CHECK_GVA_DI
	},
	[0xA5] = {
		.op_type = VIE_OP_TYPE_MOVS,
		.op_flags = VIE_OP_F_NO_MODRM | VIE_OP_F_CHECK_GVA_DI
	},
	[0xAA] = {
		.op_type = VIE_OP_TYPE_STOS,
		.op_flags = VIE_OP_F_NO_MODRM
	},
	[0xAB] = {
		.op_type = VIE_OP_TYPE_STOS,
		.op_flags = VIE_OP_F_NO_MODRM
	},
	[0xC6] = {
		/* XXX Group 11 extended opcode - not just MOV */
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0xC7] = {
		.op_type = VIE_OP_TYPE_MOV,
		.op_flags = VIE_OP_F_IMM,
	},
	[0x23] = {
		.op_type = VIE_OP_TYPE_AND,
	},
	[0x80] = {
		/* Group 1 extended opcode */
		.op_type = VIE_OP_TYPE_GROUP1,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0x81] = {
		/* Group 1 extended opcode */
		.op_type = VIE_OP_TYPE_GROUP1,
		.op_flags = VIE_OP_F_IMM,
	},
	[0x83] = {
		/* Group 1 extended opcode */
		.op_type = VIE_OP_TYPE_GROUP1,
		.op_flags = VIE_OP_F_IMM8,
	},
	[0x84] = {
		.op_type = VIE_OP_TYPE_TEST,
	},
	[0x85] = {
		.op_type = VIE_OP_TYPE_TEST,
	},
	[0x08] = {
		.op_type = VIE_OP_TYPE_OR,
	},
	[0x09] = {
		.op_type = VIE_OP_TYPE_OR,
	},
};

/* struct vie.mod */
#define	VIE_MOD_INDIRECT		0U
#define	VIE_MOD_INDIRECT_DISP8		1U
#define	VIE_MOD_INDIRECT_DISP32		2U
#define	VIE_MOD_DIRECT			3U

/* struct vie.rm */
#define	VIE_RM_SIB			4U
#define	VIE_RM_DISP32			5U

static uint64_t size2mask[9] = {
	[1] = (1UL << 8U) - 1UL,
	[2] = (1UL << 16U) - 1UL,
	[4] = (1UL << 32U) - 1UL,
	[8] = ~0UL,
};

#define VMX_INVALID_VMCS_FIELD  0xffffffffU

/*
 * This struct vmcs_seg_field is defined separately to hold the vmcs field
 * address of segment selector.
 */
struct vmcs_seg_field {
	uint32_t	base_field;
	uint32_t	limit_field;
	uint32_t	access_field;
};

static void encode_vmcs_seg_desc(enum cpu_reg_name seg,
	struct vmcs_seg_field *desc)
{
	switch (seg) {
	case CPU_REG_ES:
		desc->base_field = VMX_GUEST_ES_BASE;
		desc->limit_field = VMX_GUEST_ES_LIMIT;
		desc->access_field = VMX_GUEST_ES_ATTR;
		break;
	case CPU_REG_CS:
		desc->base_field = VMX_GUEST_CS_BASE;
		desc->limit_field = VMX_GUEST_CS_LIMIT;
		desc->access_field = VMX_GUEST_CS_ATTR;
		break;
	case CPU_REG_SS:
		desc->base_field = VMX_GUEST_SS_BASE;
		desc->limit_field = VMX_GUEST_SS_LIMIT;
		desc->access_field = VMX_GUEST_SS_ATTR;
		break;
	case CPU_REG_DS:
		desc->base_field = VMX_GUEST_DS_BASE;
		desc->limit_field = VMX_GUEST_DS_LIMIT;
		desc->access_field = VMX_GUEST_DS_ATTR;
		break;
	case CPU_REG_FS:
		desc->base_field = VMX_GUEST_FS_BASE;
		desc->limit_field = VMX_GUEST_FS_LIMIT;
		desc->access_field = VMX_GUEST_FS_ATTR;
		break;
	case CPU_REG_GS:
		desc->base_field = VMX_GUEST_GS_BASE;
		desc->limit_field = VMX_GUEST_GS_LIMIT;
		desc->access_field = VMX_GUEST_GS_ATTR;
		break;
	case CPU_REG_TR:
		desc->base_field = VMX_GUEST_TR_BASE;
		desc->limit_field = VMX_GUEST_TR_LIMIT;
		desc->access_field = VMX_GUEST_TR_ATTR;
		break;
	case CPU_REG_LDTR:
		desc->base_field = VMX_GUEST_LDTR_BASE;
		desc->limit_field = VMX_GUEST_LDTR_LIMIT;
		desc->access_field = VMX_GUEST_LDTR_ATTR;
		break;
	case CPU_REG_IDTR:
		desc->base_field = VMX_GUEST_IDTR_BASE;
		desc->limit_field = VMX_GUEST_IDTR_LIMIT;
		desc->access_field = 0xffffffffU;
		break;
	case CPU_REG_GDTR:
		desc->base_field = VMX_GUEST_GDTR_BASE;
		desc->limit_field = VMX_GUEST_GDTR_LIMIT;
		desc->access_field = 0xffffffffU;
		break;
	default:
		desc->base_field = 0U;
		desc->limit_field = 0U;
		desc->access_field = 0U;
		pr_err("%s: invalid seg %d", __func__, seg);
		break;
	}
}

/**
 *
 *Description:
 *This local function is to covert register names into
 *the corresponding field index MACROs in VMCS.
 *
 *Post Condition:
 *In the non-general register names group (CPU_REG_CR0~CPU_REG_GDTR),
 *for register names CPU_REG_CR2, CPU_REG_IDTR and CPU_REG_GDTR,
 *this function returns VMX_INVALID_VMCS_FIELD;
 *for other register names, it returns correspoding field index MACROs
 *in VMCS.
 *
 **/
static uint32_t get_vmcs_field(enum cpu_reg_name ident)
{
	switch (ident) {
	case CPU_REG_CR0:
		return VMX_GUEST_CR0;
	case CPU_REG_CR3:
		return VMX_GUEST_CR3;
	case CPU_REG_CR4:
		return VMX_GUEST_CR4;
	case CPU_REG_DR7:
		return VMX_GUEST_DR7;
	case CPU_REG_RSP:
		return VMX_GUEST_RSP;
	case CPU_REG_RIP:
		return VMX_GUEST_RIP;
	case CPU_REG_RFLAGS:
		return VMX_GUEST_RFLAGS;
	case CPU_REG_ES:
		return VMX_GUEST_ES_SEL;
	case CPU_REG_CS:
		return VMX_GUEST_CS_SEL;
	case CPU_REG_SS:
		return VMX_GUEST_SS_SEL;
	case CPU_REG_DS:
		return VMX_GUEST_DS_SEL;
	case CPU_REG_FS:
		return VMX_GUEST_FS_SEL;
	case CPU_REG_GS:
		return VMX_GUEST_GS_SEL;
	case CPU_REG_TR:
		return VMX_GUEST_TR_SEL;
	case CPU_REG_LDTR:
		return VMX_GUEST_LDTR_SEL;
	case CPU_REG_EFER:
		return VMX_GUEST_IA32_EFER_FULL;
	case CPU_REG_PDPTE0:
		return VMX_GUEST_PDPTE0_FULL;
	case CPU_REG_PDPTE1:
		return VMX_GUEST_PDPTE1_FULL;
	case CPU_REG_PDPTE2:
		return VMX_GUEST_PDPTE2_FULL;
	case CPU_REG_PDPTE3:
		return VMX_GUEST_PDPTE3_FULL;
	default: /* Never get here */
		return VMX_INVALID_VMCS_FIELD;
	}
}

/**
 * @pre vcpu != NULL
 * @pre ((reg <= CPU_REG_LAST) && (reg >= CPU_REG_FIRST))
 * @pre ((reg != CPU_REG_CR2) && (reg != CPU_REG_IDTR) && (reg != CPU_REG_GDTR))
 */
static uint64_t vm_get_register(const struct acrn_vcpu *vcpu, enum cpu_reg_name reg)
{
	uint64_t reg_val = 0UL;
	
	if ((reg >= CPU_REG_GENERAL_FIRST) && (reg <= CPU_REG_GENERAL_LAST)) {
		reg_val = vcpu_get_gpreg(vcpu, reg);
	} else if ((reg >= CPU_REG_NONGENERAL_FIRST) &&
		(reg <= CPU_REG_NONGENERAL_LAST)) {
		uint32_t field = get_vmcs_field(reg);

		if (reg <= CPU_REG_NATURAL_LAST) {
			reg_val = exec_vmread(field);
		} else if (reg <= CPU_REG_64BIT_LAST) {
			reg_val = exec_vmread64(field);
		} else {
			reg_val = (uint64_t)exec_vmread16(field);
		}
	}

	return reg_val;
}

/**
 * @pre vcpu != NULL
 * @pre ((reg <= CPU_REG_LAST) && (reg >= CPU_REG_FIRST))
 * @pre ((reg != CPU_REG_CR2) && (reg != CPU_REG_IDTR) && (reg != CPU_REG_GDTR))
 */
static void vm_set_register(struct acrn_vcpu *vcpu, enum cpu_reg_name reg,
								uint64_t val)
{

	if ((reg >= CPU_REG_GENERAL_FIRST) && (reg <= CPU_REG_GENERAL_LAST)) {
		vcpu_set_gpreg(vcpu, reg, val);
	} else if ((reg >= CPU_REG_NONGENERAL_FIRST) &&
		(reg <= CPU_REG_NONGENERAL_LAST)) {
		uint32_t field = get_vmcs_field(reg);

		if (reg <= CPU_REG_NATURAL_LAST) {
			exec_vmwrite(field, val);
		} else if (reg <= CPU_REG_64BIT_LAST) {
			exec_vmwrite64(field, val);
		} else {
			exec_vmwrite16(field, (uint16_t)val);
		}
	}
}

/**
 * @pre vcpu != NULL
 * @pre desc != NULL
 * @pre seg must be one of segment register (CPU_REG_CS/ES/DS/SS/FS/GS)
 *      or CPU_REG_TR/LDTR
 */
static void vm_get_seg_desc(enum cpu_reg_name seg, struct seg_desc *desc)
{
	struct vmcs_seg_field tdesc;

	/* tdesc->access != 0xffffffffU in this function */
	encode_vmcs_seg_desc(seg, &tdesc);

	desc->base = exec_vmread(tdesc.base_field);
	desc->limit = exec_vmread32(tdesc.limit_field);
	desc->access = exec_vmread32(tdesc.access_field);
}

static void get_guest_paging_info(struct acrn_vcpu *vcpu, struct instr_emul_ctxt *emul_ctxt,
						uint32_t csar)
{
	uint8_t cpl;

	cpl = (uint8_t)((csar >> 5U) & 3U);
	emul_ctxt->paging.cr3 = exec_vmread(VMX_GUEST_CR3);
	emul_ctxt->paging.cpl = cpl;
	emul_ctxt->paging.cpu_mode = get_vcpu_mode(vcpu);
	emul_ctxt->paging.paging_mode = get_vcpu_paging_mode(vcpu);
}

static int vie_canonical_check(enum vm_cpu_mode cpu_mode, uint64_t gla)
{
	uint64_t mask;

	if (cpu_mode != CPU_MODE_64BIT) {
		return 0;
	}

	/*
	 * The value of the bit 47 in the 'gla' should be replicated in the
	 * most significant 16 bits.
	 */
	mask = ~((1UL << 48U) - 1UL);
	if ((gla & (1UL << 47U)) != 0U) {
		return ((gla & mask) != mask) ? 1 : 0;
	} else {
		return ((gla & mask) != 0U) ? 1 : 0;
	}
}

static bool is_desc_valid(struct seg_desc *desc, uint32_t prot)
{
	uint32_t type;

	/* The descriptor type must indicate a code/data segment. */
	type = seg_desc_type(desc->access);
	if (type < 16U || type > 31U) {
		return false;
	}

	if ((prot & PROT_READ) != 0U) {
		/* #GP on a read access to a exec-only code segment */
		if ((type & 0xAU) == 0x8U) {
			return false;
		}
	}

	if ((prot & PROT_WRITE) != 0U) {
		/*
		 * #GP on a write access to a code segment or a
		 * read-only data segment.
		 */
		if ((type & 0x8U) != 0U) {	/* code segment */
			return false;
		}

		if ((type & 0xAU) == 0U) {	/* read-only data seg */
			return false;
		}
	}

	return true;
}

/*
 *@pre seg must be segment register index
 *@pre length_arg must be 1, 2, 4 or 8
 *@pre prot must be PROT_READ or PROT_WRITE
 *
 *return 0 - on success
 *return -1 - on failure
 */
static int vie_calculate_gla(enum vm_cpu_mode cpu_mode, enum cpu_reg_name seg,
	struct seg_desc *desc, uint64_t offset_arg, uint8_t addrsize,
	uint64_t *gla)
{
	uint64_t firstoff, segbase;
	uint64_t offset = offset_arg;
	uint8_t glasize;

	firstoff = offset;
	glasize = (cpu_mode == CPU_MODE_64BIT) ? 8U: 4U;

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
	firstoff &= size2mask[addrsize];
	*gla = (segbase + firstoff) & size2mask[glasize];
	return 0;
}

/*
 * @pre vcpu != NULL
 */
static inline void vie_mmio_read(const struct acrn_vcpu *vcpu, uint64_t *rval)
{
	*rval = vcpu->req.reqs.mmio.value;
}

/*
 * @pre vcpu != NULL
 */
static inline void vie_mmio_write(struct acrn_vcpu *vcpu, uint64_t wval)
{
	vcpu->req.reqs.mmio.value = wval;
}

static void vie_calc_bytereg(const struct instr_emul_vie *vie,
					enum cpu_reg_name *reg, int *lhbr)
{
	*lhbr = 0;
	*reg = vie->reg;

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
			*reg = vie->reg & 0x3U;
		}
	}
}

static uint8_t vie_read_bytereg(const struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int lhbr;
	uint64_t val;
	uint8_t reg_val;
	enum cpu_reg_name reg;

	vie_calc_bytereg(vie, &reg, &lhbr);
	val = vm_get_register(vcpu, reg);

	/*
	 * To obtain the value of a legacy high byte register shift the
	 * base register right by 8 bits (%ah = %rax >> 8).
	 */
	if (lhbr != 0) {
		reg_val = (uint8_t)(val >> 8U);
	} else {
		reg_val = (uint8_t)val;
	}

	return reg_val;
}

static void vie_write_bytereg(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie,
								uint8_t byte)
{
	uint64_t origval, val, mask;
	enum cpu_reg_name reg;
	int lhbr;

	vie_calc_bytereg(vie, &reg, &lhbr);
	origval = vm_get_register(vcpu, reg);

	val = byte;
	mask = 0xffU;
	if (lhbr != 0) {
		/*
		 * Shift left by 8 to store 'byte' in a legacy high
		 * byte register.
		 */
		val <<= 8U;
		mask <<= 8U;
	}
	val |= origval & ~mask;
	vm_set_register(vcpu, reg, val);
}

/**
 * @pre vcpu != NULL
 * @pre size = 1, 2, 4 or 8
 * @pre ((reg <= CPU_REG_LAST) && (reg >= CPU_REG_FIRST))
 * @pre ((reg != CPU_REG_CR2) && (reg != CPU_REG_IDTR) && (reg != CPU_REG_GDTR))
 */
static void vie_update_register(struct acrn_vcpu *vcpu, enum cpu_reg_name reg,
					uint64_t val_arg, uint8_t size)
{
	uint64_t origval;
	uint64_t val = val_arg;

	switch (size) {
	case 1U:
	case 2U:
		origval = vm_get_register(vcpu, reg);
		val &= size2mask[size];
		val |= origval & ~size2mask[size];
		break;
	case 4U:
		val &= 0xffffffffUL;
		break;
	default: /* size == 8 */
		break;
	}

	vm_set_register(vcpu, reg, val);
}

#define	RFLAGS_STATUS_BITS    (PSL_C | PSL_PF | PSL_AF | PSL_Z | PSL_N | PSL_V)

static void vie_update_rflags(struct acrn_vcpu *vcpu, uint64_t rflags2, uint64_t psl)
{
	uint8_t size;
	uint64_t rflags;

	rflags = vm_get_register(vcpu, CPU_REG_RFLAGS);

	rflags &= ~RFLAGS_STATUS_BITS;
	rflags |= rflags2 & psl;
	size = 8U;

	vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);
}

/*
 * Return the status flags that would result from doing (x - y).
 */
#define build_getcc(name, type)					\
static uint64_t name(type x, type y)				\
{								\
	uint64_t rflags;					\
								\
	__asm __volatile("sub %2,%1; pushfq; popq %0" :		\
			"=r" (rflags), "+r" (x) : "m" (y));	\
	return rflags;						\
}
build_getcc(getcc8, uint8_t)
build_getcc(getcc16, uint16_t)
build_getcc(getcc32, uint32_t)
build_getcc(getcc64, uint64_t)

/**
 * @pre opsize = 1, 2, 4 or 8
 */
static uint64_t getcc(uint8_t opsize, uint64_t x, uint64_t y)
{
	switch (opsize) {
	case 1U:
		return getcc8((uint8_t) x, (uint8_t) y);
	case 2U:
		return getcc16((uint16_t) x, (uint16_t) y);
	case 4U:
		return getcc32((uint32_t) x, (uint32_t) y);
	default:	/* opsize == 8 */
		return getcc64(x, y);
	}
}

static int emulate_mov(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint8_t byte;
	uint64_t val;

	size = vie->opsize;
	error = 0;
	switch (vie->opcode) {
	case 0x88U:
	/*
	 * MOV byte from reg (ModRM:reg) to mem (ModRM:r/m)
	 * 88/r:	mov r/m8, r8
	 * REX + 88/r:	mov r/m8, r8 (%ah, %ch, %dh, %bh not available)
	 */
		size = 1U;	/* override for byte operation */
		byte = vie_read_bytereg(vcpu, vie);
		vie_mmio_write(vcpu, byte);
		break;
	case 0x89U:
		/*
		 * MOV from reg (ModRM:reg) to mem (ModRM:r/m)
		 * 89/r:	mov r/m16, r16
		 * 89/r:	mov r/m32, r32
		 * REX.W + 89/r	mov r/m64, r64
		 */

		reg = vie->reg;
		val = vm_get_register(vcpu, reg);
		val &= size2mask[size];
		vie_mmio_write(vcpu, val);
		break;
	case 0x8AU:
		/*
		 * MOV byte from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8A/r:	mov r8, r/m8
		 * REX + 8A/r:	mov r8, r/m8
		 */
		size = 1U;	/* override for byte operation */
		vie_mmio_read(vcpu, &val);
		vie_write_bytereg(vcpu, vie, (uint8_t)val);
		break;
	case 0x8BU:
		/*
		 * MOV from mem (ModRM:r/m) to reg (ModRM:reg)
		 * 8B/r:	mov r16, r/m16
		 * 8B/r:	mov r32, r/m32
		 * REX.W 8B/r:	mov r64, r/m64
		 */
		vie_mmio_read(vcpu, &val);
		reg = vie->reg;
		vie_update_register(vcpu, reg, val, size);
		break;
	case 0xA1U:
		/*
		 * MOV from seg:moffset to AX/EAX/RAX
		 * A1:		mov AX, moffs16
		 * A1:		mov EAX, moffs32
		 * REX.W + A1:	mov RAX, moffs64
		 */
		vie_mmio_read(vcpu, &val);
		reg = CPU_REG_RAX;
		vie_update_register(vcpu, reg, val, size);
		break;
	case 0xA3U:
		/*
		 * MOV from AX/EAX/RAX to seg:moffset
		 * A3:		mov moffs16, AX
		 * A3:		mov moffs32, EAX
		 * REX.W + A3:	mov moffs64, RAX
		 */
		val = vm_get_register(vcpu, CPU_REG_RAX);
		val &= size2mask[size];
		vie_mmio_write(vcpu, val);
		break;
	case 0xC6U:
		/*
		 * MOV from imm8 to mem (ModRM:r/m)
		 * C6/0		mov r/m8, imm8
		 * REX + C6/0	mov r/m8, imm8
		 */
		size = 1U;	/* override for byte operation */
		vie_mmio_write(vcpu, (uint64_t)vie->immediate);
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
		vie_mmio_write(vcpu, val);
		break;
	default:
		/*
		 * For the opcode that is not handled (an invalid opcode), the
		 * error code is assigned to a default value (-EINVAL).
		 * Gracefully return this error code if prior case clauses have
		 * not been met.
		 */
		error = -EINVAL;
		break;
	}

	return error;
}

static int emulate_movx(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t val;

	size = vie->opsize;
	error = 0;

	switch (vie->opcode) {
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
		vie_mmio_read(vcpu, &val);

		/* get the second operand */
		reg = vie->reg;

		/* zero-extend byte */
		val = (uint8_t)val;

		/* write the result */
		vie_update_register(vcpu, reg, val, size);
		break;
	case 0xB7U:
		/*
		 * MOV and zero extend word from mem (ModRM:r/m) to
		 * reg (ModRM:reg).
		 *
		 * 0F B7/r		movzx r32, r/m16
		 * REX.W + 0F B7/r	movzx r64, r/m16
		 */
		vie_mmio_read(vcpu, &val);

		reg = vie->reg;

		/* zero-extend word */
		val = (uint16_t)val;

		vie_update_register(vcpu, reg, val, size);
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
		vie_mmio_read(vcpu, &val);

		/* get the second operand */
		reg = vie->reg;

		/* sign extend byte */
		val = (int8_t)val;

		/* write the result */
		vie_update_register(vcpu, reg, val, size);
		break;
	default:
		/*
		 * For the opcode that is not handled (an invalid opcode), the
		 * error code is assigned to a default value (-EINVAL).
		 * Gracefully return this error code if prior case clauses have
		 * not been met.
		 */
		error = -EINVAL;
		break;
	}
	return error;
}

/**
 * @pre only called by instruction emulation and check was done during
 *      instruction decode
 *
 * @remark This function can only be called in instruction emulation and
 * suppose always success because the check was done during instruction
 * decode.
 *
 * It's only used by MOVS/STO
 */
static void get_gva_si_nocheck(const struct acrn_vcpu *vcpu, uint8_t addrsize,
		enum cpu_reg_name seg, uint64_t *gva)
{
	uint64_t val;
	struct seg_desc desc;
	enum vm_cpu_mode cpu_mode;

	val = vm_get_register(vcpu, CPU_REG_RSI);
	vm_get_seg_desc(seg, &desc);
	cpu_mode = get_vcpu_mode(vcpu);

	(void)vie_calculate_gla(cpu_mode, seg, &desc, val, addrsize, gva);

	return;
}

/*
 * @pre only called during instruction decode phase
 *
 * @remark This function get gva from ES:DI. And do check the failure
 * condition and inject exception to guest accordingly.
 *
 * It's only used by MOVS/STO
 */
static int get_gva_di_check(struct acrn_vcpu *vcpu, struct instr_emul_vie *vie,
		uint8_t addrsize, uint64_t *gva)
{
	int ret;
	uint32_t err_code;
	struct seg_desc desc;
	enum vm_cpu_mode cpu_mode;
	uint64_t val, gpa;

	vm_get_seg_desc(CPU_REG_ES, &desc);
	cpu_mode = get_vcpu_mode(vcpu);

	if (cpu_mode == CPU_MODE_64BIT) {
		if ((addrsize != 4U) && (addrsize != 8U)) {
			goto exception_inject;
		}
	} else {
		if ((addrsize != 2U) && (addrsize != 4U)) {
			goto exception_inject;
		}

		if (!is_desc_valid(&desc, PROT_WRITE)) {
			goto exception_inject;
		}
	}

	val = vm_get_register(vcpu, CPU_REG_RDI);
	if (vie_calculate_gla(cpu_mode, CPU_REG_ES, &desc, val, addrsize, gva) != 0) {
		goto exception_inject;
	}

	if (vie_canonical_check(cpu_mode, *gva) != 0) {
		goto exception_inject;
	}

	err_code = PAGE_FAULT_WR_FLAG;
	ret = gva2gpa(vcpu, *gva, &gpa, &err_code);
	if (ret < 0) {
		if (ret == -EFAULT) {
			vcpu_inject_pf(vcpu, (uint64_t)gva, err_code);
		}
		return ret;
	}

	/* If we are checking the dest operand for movs instruction,
	 * we cache the gpa if check pass. It will be used during
	 * movs instruction emulation.
	 */
	vie->dst_gpa = gpa;

	return 0;

exception_inject:
	vcpu_inject_gp(vcpu, 0U);
	return -EFAULT;
}

/* MOVs gets the operands from RSI and RDI. Both operands could be memory.
 * With VMX enabled, one of the operand triggers EPT voilation.
 *
 * If it's RSI access trigger EPT voilation, it's source operands and always
 * read operations. Not neccesary to check whether need to inject fault (done
 * by VMX already). We do need to check the RDI.
 *
 * If it's RDI access trigger EPT voilation, we need to check RDI because it's
 * always write operations and VMX doens't cover write access check.
 * Not neccesary to check RSI, because VMX cover it for us.
 *
 * In summary,
 * For MOVs instruction, we always check RDI during instruction decoding phase.
 * And access RSI without any check during instruction emulation phase.
 */
static int emulate_movs(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	uint64_t src_gva, gpa, val = 0UL;
	uint64_t *dst_hva, *src_hva;
	uint64_t rcx, rdi, rsi, rflags;
	uint32_t err_code;
	enum cpu_reg_name seg;
	int error, repeat;
	uint8_t opsize = vie->opsize;
	bool is_mmio_write;

	error = 0;
	is_mmio_write = (vcpu->req.reqs.mmio.direction == REQUEST_WRITE);

	/*
	 * XXX although the MOVS instruction is only supposed to be used with
	 * the "rep" prefix some guests like FreeBSD will use "repnz" instead.
	 *
	 * Empirically the "repnz" prefix has identical behavior to "rep"
	 * and the zero flag does not make a difference.
	 */
	repeat = vie->repz_present | vie->repnz_present;

	if (repeat != 0) {
		rcx = vm_get_register(vcpu, CPU_REG_RCX);

		/*
		 * The count register is %rcx, %ecx or %cx depending on the
		 * address size of the instruction.
		 */
		if ((rcx & size2mask[vie->addrsize]) == 0UL) {
			error = 0;
			goto done;
		}
	}

	seg = (vie->seg_override != 0U) ? (vie->segment_register) : CPU_REG_DS;

	if (is_mmio_write) {
		get_gva_si_nocheck(vcpu, vie->addrsize, seg, &src_gva);

		/* we are sure it will success */
		(void)gva2gpa(vcpu, src_gva, &gpa, &err_code);
		src_hva = gpa2hva(vcpu->vm, gpa);
		(void)memcpy_s(&val, opsize, src_hva, opsize);

		vie_mmio_write(vcpu, val);
	} else {
		vie_mmio_read(vcpu, &val);

		/* The dest gpa is saved during dst check instruction
		 * decoding.
		 */
		dst_hva = gpa2hva(vcpu->vm, vie->dst_gpa);
		(void)memcpy_s(dst_hva, opsize, &val, opsize);
	}

	rsi = vm_get_register(vcpu, CPU_REG_RSI);
	rdi = vm_get_register(vcpu, CPU_REG_RDI);
	rflags = vm_get_register(vcpu, CPU_REG_RFLAGS);

	if ((rflags & PSL_D) != 0U) {
		rsi -= opsize;
		rdi -= opsize;
	} else {
		rsi += opsize;
		rdi += opsize;
	}

	vie_update_register(vcpu, CPU_REG_RSI, rsi, vie->addrsize);
	vie_update_register(vcpu, CPU_REG_RDI, rdi, vie->addrsize);

	if (repeat != 0) {
		rcx = rcx - 1;
		vie_update_register(vcpu, CPU_REG_RCX, rcx, vie->addrsize);

		/*
		 * Repeat the instruction if the count register is not zero.
		 */
		if ((rcx & size2mask[vie->addrsize]) != 0UL) {
			vcpu_retain_rip(vcpu);
		}
	}
done:
	return error;
}

static int emulate_stos(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int repeat;
	uint8_t opsize = vie->opsize;
	uint64_t val;
	uint64_t rcx, rdi, rflags;

	repeat = vie->repz_present | vie->repnz_present;

	if (repeat != 0) {
		rcx = vm_get_register(vcpu, CPU_REG_RCX);

		/*
		 * The count register is %rcx, %ecx or %cx depending on the
		 * address size of the instruction.
		 */
		if ((rcx & size2mask[vie->addrsize]) == 0UL) {
			return 0;
		}
	}

	val = vm_get_register(vcpu, CPU_REG_RAX);

	vie_mmio_write(vcpu, val);

	rdi = vm_get_register(vcpu, CPU_REG_RDI);
	rflags = vm_get_register(vcpu, CPU_REG_RFLAGS);

	if ((rflags & PSL_D) != 0U) {
		rdi -= opsize;
	} else {
		rdi += opsize;
	}

	vie_update_register(vcpu, CPU_REG_RDI, rdi, vie->addrsize);

	if (repeat != 0) {
		rcx = rcx - 1;
		vie_update_register(vcpu, CPU_REG_RCX, rcx, vie->addrsize);

		/*
		 * Repeat the instruction if the count register is not zero.
		 */
		if ((rcx & size2mask[vie->addrsize]) != 0UL) {
			vcpu_retain_rip(vcpu);
		}
	}

	return 0;
}

static int emulate_test(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t result, rflags2, val1, val2;

	size = vie->opsize;
	error = 0;

	switch (vie->opcode) {
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
		reg = vie->reg;
		val1 = vm_get_register(vcpu, reg);

		/* get the second operand */
		vie_mmio_read(vcpu, &val2);

		/* perform the operation and write the result */
		result = val1 & val2;
		break;
	default:
		/*
		 * For the opcode that is not handled (an invalid opcode), the
		 * error code is assigned to a default value (-EINVAL).
		 * Gracefully return this error code if prior case clauses have
		 * not been met.
		 */
		error = -EINVAL;
		break;
	}

	if (error == 0) {
		/*
		 * OF and CF are cleared; the SF, ZF and PF flags are set
		 * according to the result; AF is undefined.
		 *
		 * The updated status flags are obtained by subtracting 0 from
		 * 'result'.
		 */
		rflags2 = getcc(size, result, 0UL);
		vie_update_rflags(vcpu, rflags2, PSL_PF | PSL_Z | PSL_N);
	}

	return error;
}

static int emulate_and(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t result, rflags2, val1, val2;

	size = vie->opsize;
	error = 0;

	switch (vie->opcode) {
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
		reg = vie->reg;
		val1 = vm_get_register(vcpu, reg);

		/* get the second operand */
		vie_mmio_read(vcpu, &val2);

		/* perform the operation and write the result */
		result = val1 & val2;
		vie_update_register(vcpu, reg, result, size);
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
		vie_mmio_read(vcpu, &val1);

		/*
		 * perform the operation with the pre-fetched immediate
		 * operand and write the result
		 */
		result = val1 & vie->immediate;
		vie_mmio_write(vcpu, result);
		break;
	default:
		/*
		 * For the opcode that is not handled (an invalid opcode), the
		 * error code is assigned to a default value (-EINVAL).
		 * Gracefully return this error code if prior case clauses have
		 * not been met.
		 */
		error = -EINVAL;
		break;
	}

	if (error == 0) {
		/*
		 * OF and CF are cleared; the SF, ZF and PF flags are set
		 * according to the result; AF is undefined.
		 *
		 * The updated status flags are obtained by subtracting 0 from
		 * 'result'.
		 */
		rflags2 = getcc(size, result, 0UL);
		vie_update_rflags(vcpu, rflags2, PSL_PF | PSL_Z | PSL_N);
	}

	return error;
}

static int emulate_or(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	enum cpu_reg_name reg;
	uint64_t val1, val2, result, rflags2;

	size = vie->opsize;
	error = 0;

	switch (vie->opcode) {
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
		vie_mmio_read(vcpu, &val1);

		/*
		 * perform the operation with the pre-fetched immediate
		 * operand and write the result
		 */
		result = val1 | (uint64_t)vie->immediate;
		vie_mmio_write(vcpu, result);
		break;
	case 0x09U:
		/*
		 * OR mem (ModRM:r/m) with reg (ModRM:reg) and store the
		 * result in mem.
		 * 09/r:    OR r/m16, r16
		 * 09/r:    OR r/m32, r32
		 */

		/* get the first operand */
		vie_mmio_read(vcpu, &val1);

		/* get the second operand */
		reg = vie->reg;
		val2 = vm_get_register(vcpu, reg);

		/* perform the operation and write the result */
		result = val1 | val2;
		result &= size2mask[size];

		vie_mmio_write(vcpu, result);
		break;
	default:
		/*
		 * For the opcode that is not handled (an invalid opcode), the
		 * error code is assigned to a default value (-EINVAL).
		 * Gracefully return this error code if prior case clauses have
		 * not been met.
		 */
		error = -EINVAL;
		break;
	}
	if (error == 0) {
		/*
		 * OF and CF are cleared; the SF, ZF and PF flags are set
		 * according to the result; AF is undefined.
		 *
		 * The updated status flags are obtained by subtracting 0 from
		 * 'result'.
		 */
		rflags2 = getcc(size, result, 0UL);
		vie_update_rflags(vcpu, rflags2, PSL_PF | PSL_Z | PSL_N);
	}

	return error;
}

static int emulate_cmp(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	uint64_t regop, memop, op1, op2, rflags2;
	enum cpu_reg_name reg;

	size = vie->opsize;
	switch (vie->opcode) {
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
		reg = vie->reg;
		regop = vm_get_register(vcpu, reg);

		/* Get the memory operand */
		vie_mmio_read(vcpu, &memop);

		if (vie->opcode == 0x3BU) {
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
		if (vie->opcode == 0x80U) {
			size = 1U;
		}

		/* get the first operand */
		vie_mmio_read(vcpu, &op1);

		rflags2 = getcc(size, op1, (uint64_t)vie->immediate);
		break;
	default:
		return -EINVAL;
	}

	vie_update_rflags(vcpu, rflags2, RFLAGS_STATUS_BITS);

	return error;
}

static int emulate_sub(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;
	uint8_t size;
	uint64_t nval, rflags2, val1, val2;
	enum cpu_reg_name reg;

	size = vie->opsize;
	error = 0;

	switch (vie->opcode) {
	case 0x2BU:
		/*
		 * SUB r/m from r and store the result in r
		 *
		 * 2B/r            SUB r16, r/m16
		 * 2B/r            SUB r32, r/m32
		 * REX.W + 2B/r    SUB r64, r/m64
		 */

		/* get the first operand */
		reg = vie->reg;
		val1 = vm_get_register(vcpu, reg);

		/* get the second operand */
		vie_mmio_read(vcpu, &val2);

		/* perform the operation and write the result */
		nval = val1 - val2;
		vie_update_register(vcpu, reg, nval, size);
		break;
	default:
		/*
		 * For the opcode that is not handled (an invalid opcode), the
		 * error code is assigned to a default value (-EINVAL).
		 * Gracefully return this error code if prior case clauses have
		 * not been met.
		 */
		error = -EINVAL;
		break;
	}

	if (error == 0) {
		rflags2 = getcc(size, val1, val2);
		vie_update_rflags(vcpu, rflags2, RFLAGS_STATUS_BITS);
	}

	return error;
}

static int emulate_group1(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	int error;

	switch (vie->reg & 7U) {
	case 0x1U:	/* OR */
		error = emulate_or(vcpu, vie);
		break;
	case 0x4U:	/* AND */
		error = emulate_and(vcpu, vie);
		break;
	case 0x7U:	/* CMP */
		error = emulate_cmp(vcpu, vie);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static int32_t emulate_bittest(struct acrn_vcpu *vcpu, const struct instr_emul_vie *vie)
{
	uint64_t val, rflags, bitmask;
	uint64_t bitoff;
	uint8_t size;
	int32_t ret;

	/*
	 * 0F BA is a Group 8 extended opcode.
	 *
	 * Currently we only emulate the 'Bit Test' instruction which is
	 * identified by a ModR/M:reg encoding of 100b.
	 */
	if ((vie->reg & 7U) == 4U) {
		rflags = vm_get_register(vcpu, CPU_REG_RFLAGS);

		vie_mmio_read(vcpu, &val);

		/*
		 * Intel SDM, Vol 2, Table 3-2:
		 * "Range of Bit Positions Specified by Bit Offset Operands"
		 */
		bitmask = ((uint64_t)vie->opsize * 8UL) - 1UL;
		bitoff = (uint64_t)vie->immediate & bitmask;

		/* Copy the bit into the Carry flag in %rflags */
		if ((val & (1UL << bitoff)) != 0U) {
			rflags |= PSL_C;
		} else {
			rflags &= ~PSL_C;
		}
		size = 8U;
		vie_update_register(vcpu, CPU_REG_RFLAGS, rflags, size);
		ret = 0;
	} else {
	        ret = -EINVAL;
	}

	return ret;
}

static int vmm_emulate_instruction(struct instr_emul_ctxt *ctxt)
{
	struct instr_emul_vie *vie = &ctxt->vie;
	struct acrn_vcpu *vcpu = ctxt->vcpu;
	int error;

	if (vie->decoded != 0U) {
		switch (vie->op.op_type) {
		case VIE_OP_TYPE_GROUP1:
			error = emulate_group1(vcpu, vie);
			break;
		case VIE_OP_TYPE_CMP:
			error = emulate_cmp(vcpu, vie);
			break;
		case VIE_OP_TYPE_MOV:
			error = emulate_mov(vcpu, vie);
			break;
		case VIE_OP_TYPE_MOVSX:
		case VIE_OP_TYPE_MOVZX:
			error = emulate_movx(vcpu, vie);
			break;
		case VIE_OP_TYPE_MOVS:
			error = emulate_movs(vcpu, vie);
			break;
		case VIE_OP_TYPE_STOS:
			error = emulate_stos(vcpu, vie);
			break;
		case VIE_OP_TYPE_AND:
			error = emulate_and(vcpu, vie);
			break;
		case VIE_OP_TYPE_TEST:
			error = emulate_test(vcpu, vie);
			break;
		case VIE_OP_TYPE_OR:
			error = emulate_or(vcpu, vie);
			break;
		case VIE_OP_TYPE_SUB:
			error = emulate_sub(vcpu, vie);
			break;
		case VIE_OP_TYPE_BITTEST:
			error = emulate_bittest(vcpu, vie);
			break;
		default:
			error = -EINVAL;
			break;
		}
	} else {
		error = -EINVAL;
	}

	return error;
}

static int vie_init(struct instr_emul_vie *vie, struct acrn_vcpu *vcpu)
{
	uint64_t guest_rip_gva = vcpu_get_rip(vcpu);
	uint32_t inst_len = vcpu->arch.inst_len;
	uint32_t err_code;
	uint64_t fault_addr;
	int ret;

	if ((inst_len > VIE_INST_SIZE) || (inst_len == 0U)) {
		pr_err("%s: invalid instruction length (%d)",
			__func__, inst_len);
		return -EINVAL;
	}

	(void)memset(vie, 0U, sizeof(struct instr_emul_vie));

	/* init register fields in vie. */
	vie->base_register = CPU_REG_LAST;
	vie->index_register = CPU_REG_LAST;
	vie->segment_register = CPU_REG_LAST;

	err_code = PAGE_FAULT_ID_FLAG;
	ret = copy_from_gva(vcpu, vie->inst, guest_rip_gva,
				inst_len, &err_code, &fault_addr);
	if (ret < 0) {
		if (ret == -EFAULT) {
			vcpu_inject_pf(vcpu, fault_addr, err_code);
		}
		return ret;
	}

	vie->num_valid = (uint8_t)inst_len;

	return 0;
}

static int vie_peek(const struct instr_emul_vie *vie, uint8_t *x)
{

	if (vie->num_processed < vie->num_valid) {
		*x = vie->inst[vie->num_processed];
		return 0;
	} else {
		return -1;
	}
}

static void vie_advance(struct instr_emul_vie *vie)
{

	vie->num_processed++;
}

static bool segment_override(uint8_t x, enum cpu_reg_name *seg)
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

static int decode_prefixes(struct instr_emul_vie *vie,
					enum vm_cpu_mode cpu_mode, bool cs_d)
{
	uint8_t x, i;

	for (i = 0U; i < VIE_PREFIX_SIZE; i++) {
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
			vie->seg_override = 1U;
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
	if ((cpu_mode == CPU_MODE_64BIT) && (x >= 0x40U) && (x <= 0x4FU)) {
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

static int decode_two_byte_opcode(struct instr_emul_vie *vie)
{
	uint8_t x;

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	vie->opcode = x;
	vie->op = two_byte_opcodes[x];

	if (vie->op.op_type == VIE_OP_TYPE_NONE) {
		return -1;
	}

	vie_advance(vie);

	return 0;
}

static int decode_opcode(struct instr_emul_vie *vie)
{
	int ret = 0;
	uint8_t x;

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	vie->opcode = x;
	vie->op = one_byte_opcodes[x];

	if (vie->op.op_type == VIE_OP_TYPE_NONE) {
		return -1;
	}

	vie_advance(vie);

	if (vie->op.op_type == VIE_OP_TYPE_TWO_BYTE) {
		ret = decode_two_byte_opcode(vie);
	}

	/* Fixup the opsize according to opcode w bit:
	 * If w bit of opcode is 0, the operand size is 1 byte
	 * If w bit of opcode is 1, the operand size is decided
	 * by prefix and default operand size attribute (handled
	 * in decode_prefixes).
	 */
	if ((ret == 0) && ((vie->opcode & 0x1U) == 0U)) {
		vie->opsize = 1U;
	}

	return ret;
}

static int decode_modrm(struct instr_emul_vie *vie, enum vm_cpu_mode cpu_mode)
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

	vie->mod = (x >> 6U) & 0x3U;
	vie->rm =  (x >> 0U) & 0x7U;
	vie->reg = (x >> 3U) & 0x7U;

	/*
	 * A direct addressing mode makes no sense in the context of an EPT
	 * fault. There has to be a memory access involved to cause the
	 * EPT fault.
	 */
	if (vie->mod == VIE_MOD_DIRECT) {
		return -1;
	}

	if (((vie->mod == VIE_MOD_INDIRECT) && (vie->rm == VIE_RM_DISP32)) ||
			((vie->mod != VIE_MOD_DIRECT) && (vie->rm == VIE_RM_SIB))) {
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
		vie->rm |= (vie->rex_b << 3U);
	}

	vie->reg |= (vie->rex_r << 3U);

	/* SIB */
	if (vie->mod != VIE_MOD_DIRECT && vie->rm == VIE_RM_SIB) {
		goto done;
	}

	vie->base_register = vie->rm;

	switch (vie->mod) {
	case VIE_MOD_INDIRECT_DISP8:
		vie->disp_bytes = 1U;
		break;
	case VIE_MOD_INDIRECT_DISP32:
		vie->disp_bytes = 4U;
		break;
	case VIE_MOD_INDIRECT:
		if (vie->rm == VIE_RM_DISP32) {
			vie->disp_bytes = 4U;
		/*
		 * Table 2-7. RIP-Relative Addressing
		 *
		 * In 64-bit mode mod=00 r/m=101 implies [rip] + disp32
		 * whereas in compatibility mode it just implies disp32.
		 */

			if (cpu_mode == CPU_MODE_64BIT) {
				vie->base_register = CPU_REG_RIP;
				pr_err("VM exit with RIP as indirect access");
			}
			else {
				vie->base_register = CPU_REG_LAST;
			}
		}
		break;
	default:
		/* VIE_MOD_DIRECT */
		break;
	}

done:
	vie_advance(vie);

	return 0;
}

static int decode_sib(struct instr_emul_vie *vie)
{
	uint8_t x;

	/* Proceed only if SIB byte is present */
	if ((vie->mod == VIE_MOD_DIRECT) || (vie->rm != VIE_RM_SIB)) {
		return 0;
	}

	if (vie_peek(vie, &x) != 0) {
		return -1;
	}

	/* De-construct the SIB byte */
	vie->ss = (x >> 6U) & 0x3U;
	vie->index = (x >> 3U) & 0x7U;
	vie->base = (x >> 0U) & 0x7U;

	/* Apply the REX prefix modifiers */
	vie->index |= vie->rex_x << 3U;
	vie->base |= vie->rex_b << 3U;

	switch (vie->mod) {
	case VIE_MOD_INDIRECT_DISP8:
		vie->disp_bytes = 1U;
		break;
	case VIE_MOD_INDIRECT_DISP32:
		vie->disp_bytes = 4U;
		break;
	default:
		/*
		 * All possible values of 'vie->mod':
		 * 1. VIE_MOD_DIRECT
		 *    has been handled at the start of this function
		 * 2. VIE_MOD_INDIRECT_DISP8
		 *    has been handled in prior case clauses
		 * 3. VIE_MOD_INDIRECT_DISP32
		 *    has been handled in prior case clauses
		 * 4. VIE_MOD_INDIRECT
		 *    will be handled later after this switch statement
		 */
		break;
	}

	if ((vie->mod == VIE_MOD_INDIRECT) &&
			((vie->base == 5U) || (vie->base == 13U))) {
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
		vie->base_register = vie->base;
	}

	/*
	 * All encodings of 'index' are valid except for %rsp (4).
	 *
	 * Documented in:
	 * Table 2-3: 32-bit Addressing Forms with the SIB Byte
	 * Table 2-5: Special Cases of REX Encodings
	 */
	if (vie->index != 4U) {
		vie->index_register = vie->index;
	}

	/* 'scale' makes sense only in the context of an index register */
	if (vie->index_register < CPU_REG_LAST) {
		vie->scale = 1U << vie->ss;
	}

	vie_advance(vie);

	return 0;
}

static int decode_displacement(struct instr_emul_vie *vie)
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

	if ((n != 1) && (n != 4)) {
		pr_err("%s: decode_displacement: invalid disp_bytes %d",
			__func__, n);
		return -EINVAL;
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

static int decode_immediate(struct instr_emul_vie *vie)
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
		if ((vie->opsize == 4U) || (vie->opsize == 8U)) {
			vie->imm_bytes = 4U;
		}
		else {
			vie->imm_bytes = 2U;
		}
	} else if ((vie->op.op_flags & VIE_OP_F_IMM8) != 0U) {
		vie->imm_bytes = 1U;
	} else {
		/* No op_flag on immediate operand size */
	}

	n = vie->imm_bytes;
	if (n == 0) {
		return 0;
	}

	if ((n != 1) && (n != 2) && (n != 4)) {
		pr_err("%s: invalid number of immediate bytes: %d",
			__func__, n);
		return -EINVAL;
	}

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

static int decode_moffset(struct instr_emul_vie *vie)
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
	if ((n != 2U) && (n != 4U) && (n != 8U)) {
		pr_err("%s: invalid moffset bytes: %hhu", __func__, n);
		return -EINVAL;
	}

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

static int local_decode_instruction(enum vm_cpu_mode cpu_mode,
				bool cs_d, struct instr_emul_vie *vie)
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

/* for instruction MOVS/STO, check the gva gotten from DI/SI. */
static int32_t instr_check_di(struct acrn_vcpu *vcpu, struct instr_emul_ctxt *emul_ctxt)
{
	int32_t ret;
	struct instr_emul_vie *vie = &emul_ctxt->vie;
	uint64_t gva;

	ret = get_gva_di_check(vcpu, vie, vie->addrsize, &gva);

	if (ret < 0) {
		ret = -EFAULT;
	} else {
		ret = 0;
	}

	return ret;
}

static int instr_check_gva(struct acrn_vcpu *vcpu, struct instr_emul_ctxt *emul_ctxt,
		enum vm_cpu_mode cpu_mode)
{
	int ret;
	uint64_t base, segbase, idx, gva, gpa;
	uint32_t err_code;
	enum cpu_reg_name seg;
	struct instr_emul_vie *vie = &emul_ctxt->vie;

	base = 0UL;
	if (vie->base_register != CPU_REG_LAST) {
		base = vm_get_register(vcpu, vie->base_register);

		/* RIP relative addressing starts from the
		 * following instruction
		 */
		if (vie->base_register == CPU_REG_RIP) {
			base += vie->num_processed;
		}

	}

	idx = 0UL;
	if (vie->index_register != CPU_REG_LAST) {
		idx = vm_get_register(vcpu, vie->index_register);
	}

	/* "Specifying a Segment Selector" of SDM Vol1 3.7.4
	 *
	 * In legacy IA-32 mode, when ESP or EBP register is used as
	 * base, the SS segment is default segment.
	 *
	 * All data references, except when relative to stack or
	 * string destination, DS is default segment.
	 *
	 * segment override could overwrite the default segment
	 *
	 * 64bit mode, segmentation is generally disabled. The
	 * exception are FS and GS.
	 */
	if (vie->seg_override != 0U) {
		seg = vie->segment_register;
	} else if ((vie->base_register == CPU_REG_RSP) ||
			(vie->base_register == CPU_REG_RBP)) {
		seg = CPU_REG_SS;
	} else {
		seg = CPU_REG_DS;
	}

	if ((cpu_mode == CPU_MODE_64BIT) && (seg != CPU_REG_FS) &&
			(seg != CPU_REG_GS)) {
		segbase = 0UL;
	} else {
		struct seg_desc desc;

		vm_get_seg_desc(seg, &desc);

		segbase = desc.base;
	}

	gva = segbase + base + (uint64_t)vie->scale * idx + (uint64_t)vie->displacement;

	if (vie_canonical_check(cpu_mode, gva) != 0) {
		if (seg == CPU_REG_SS) {
			vcpu_inject_ss(vcpu);
		} else {
			vcpu_inject_gp(vcpu, 0U);
		}
		return -EFAULT;
	}

	err_code = (vcpu->req.reqs.mmio.direction == REQUEST_WRITE) ?
	       PAGE_FAULT_WR_FLAG : 0U;

	ret = gva2gpa(vcpu, gva, &gpa, &err_code);
	if (ret < 0) {
		if (ret == -EFAULT) {
			vcpu_inject_pf(vcpu, gva,
					err_code);
		}
		return ret;
	}

	return 0;
}

int decode_instruction(struct acrn_vcpu *vcpu)
{
	struct instr_emul_ctxt *emul_ctxt;
	uint32_t csar;
	int retval;
	enum vm_cpu_mode cpu_mode;

	emul_ctxt = &per_cpu(g_inst_ctxt, vcpu->pcpu_id);
	if (emul_ctxt == NULL) {
		pr_err("%s: Failed to get emul_ctxt", __func__);
		return -1;
	}
	emul_ctxt->vcpu = vcpu;

	retval = vie_init(&emul_ctxt->vie, vcpu);
	if (retval < 0) {
		if (retval != -EFAULT) {
			pr_err("init vie failed @ 0x%016llx:",
				vcpu_get_rip(vcpu));
		}
		return retval;
	}

	csar = exec_vmread32(VMX_GUEST_CS_ATTR);
	get_guest_paging_info(vcpu, emul_ctxt, csar);
	cpu_mode = get_vcpu_mode(vcpu);

	retval = local_decode_instruction(cpu_mode, seg_desc_def32(csar),
		&emul_ctxt->vie);

	if (retval != 0) {
		pr_err("decode instruction failed @ 0x%016llx:",
			vcpu_get_rip(vcpu));
		vcpu_inject_ud(vcpu);
		return -EFAULT;
	}

	/*
	 * We do operand check in instruction decode phase and
	 * inject exception accordingly. In late instruction
	 * emulation, it will always sucess.
	 *
	 * We only need to do dst check for movs. For other instructions,
	 * they always has one register and one mmio which trigger EPT
	 * by access mmio. With VMX enabled, the related check is done
	 * by VMX itself before hit EPT violation.
	 *
	 */
	if ((emul_ctxt->vie.op.op_flags & VIE_OP_F_CHECK_GVA_DI) != 0U) {
		retval = instr_check_di(vcpu, emul_ctxt);
		if (retval < 0) {
			return retval;
		}
	} else {
		retval = instr_check_gva(vcpu, emul_ctxt, cpu_mode);
		if (retval < 0) {
			return retval;
		}
	}

	return (int)(emul_ctxt->vie.opsize);
}

int32_t emulate_instruction(const struct acrn_vcpu *vcpu)
{
	struct instr_emul_ctxt *ctxt = &per_cpu(g_inst_ctxt, vcpu->pcpu_id);
	int32_t ret;

	if (ctxt == NULL) {
		pr_err("%s: Failed to get instr_emul_ctxt", __func__);
		ret = -1;
	} else {
		ret = vmm_emulate_instruction(ctxt); 
	}

	return ret;
}
