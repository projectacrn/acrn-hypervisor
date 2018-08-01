/*-
 * Copyright (c) 2011 NetApp, Inc.
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

#ifndef _VMM_H_
#define	_VMM_H_

#include "types.h"
#include "vhm_ioctl_defs.h"

/*
 * Entries in the Interrupt Descriptor Table (IDT)
 */
#define IDT_DE		0	/* #DE: Divide Error */
#define IDT_DB		1	/* #DB: Debug */
#define IDT_NMI		2	/* Nonmaskable External Interrupt */
#define IDT_BP		3	/* #BP: Breakpoint */
#define IDT_OF		4	/* #OF: Overflow */
#define IDT_BR		5	/* #BR: Bound Range Exceeded */
#define IDT_UD		6	/* #UD: Undefined/Invalid Opcode */
#define IDT_NM		7	/* #NM: No Math Coprocessor */
#define IDT_DF		8	/* #DF: Double Fault */
#define IDT_FPUGP	9	/* Coprocessor Segment Overrun */
#define IDT_TS		10	/* #TS: Invalid TSS */
#define IDT_NP		11	/* #NP: Segment Not Present */
#define IDT_SS		12	/* #SS: Stack Segment Fault */
#define IDT_GP		13	/* #GP: General Protection Fault */
#define IDT_PF		14	/* #PF: Page Fault */
#define IDT_MF		16	/* #MF: FPU Floating-Point Error */
#define IDT_AC		17	/* #AC: Alignment Check */
#define IDT_MC		18	/* #MC: Machine Check */
#define IDT_XF		19	/* #XF: SIMD Floating-Point Exception */

enum vm_suspend_how {
	VM_SUSPEND_NONE,
	VM_SUSPEND_SYSTEM_RESET,
	VM_SUSPEND_FULL_RESET,
	VM_SUSPEND_POWEROFF,
	VM_SUSPEND_SUSPEND,
	VM_SUSPEND_HALT,
	VM_SUSPEND_TRIPLEFAULT,
	VM_SUSPEND_LAST
};

/*
 * Identifiers for architecturally defined registers.
 */
enum cpu_reg_name {
	CPU_REG_RAX,
	CPU_REG_RBX,
	CPU_REG_RCX,
	CPU_REG_RDX,
	CPU_REG_RSI,
	CPU_REG_RDI,
	CPU_REG_RBP,
	CPU_REG_R8,
	CPU_REG_R9,
	CPU_REG_R10,
	CPU_REG_R11,
	CPU_REG_R12,
	CPU_REG_R13,
	CPU_REG_R14,
	CPU_REG_R15,
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

#define	VM_INTINFO_VECTOR(info)	((info) & 0xff)
#define	VM_INTINFO_DEL_ERRCODE	0x800
#define	VM_INTINFO_RSVD		0x7ffff000
#define	VM_INTINFO_VALID	0x80000000
#define	VM_INTINFO_TYPE		0x700
#define	VM_INTINFO_HWINTR	(0 << 8)
#define	VM_INTINFO_NMI		(2 << 8)
#define	VM_INTINFO_HWEXCEPTION	(3 << 8)
#define	VM_INTINFO_SWINTR	(4 << 8)

#define	VM_MAXCPU	16			/* maximum virtual cpus */

/*
 * Identifiers for optional vmm capabilities
 */
enum vm_cap_type {
	VM_CAP_HALT_EXIT,
	VM_CAP_MTRAP_EXIT,
	VM_CAP_PAUSE_EXIT,
	VM_CAP_UNRESTRICTED_GUEST,
	VM_CAP_ENABLE_INVPCID,
	VM_CAP_MAX
};

enum vm_intr_trigger {
	EDGE_TRIGGER,
	LEVEL_TRIGGER
};

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
#define	SEG_DESC_TYPE(access)		((access) & 0x001f)
#define	SEG_DESC_DPL(access)		(((access) >> 5) & 0x3)
#define	SEG_DESC_PRESENT(access)	(((access) & 0x0080) ? 1 : 0)
#define	SEG_DESC_DEF32(access)		(((access) & 0x4000) ? 1 : 0)
#define	SEG_DESC_GRANULARITY(access)	(((access) & 0x8000) ? 1 : 0)
#define	SEG_DESC_UNUSABLE(access)	(((access) & 0x10000) ? 1 : 0)

enum vm_cpu_mode {
	CPU_MODE_REAL,
	CPU_MODE_PROTECTED,
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

enum vm_paging_mode {
	PAGING_MODE_FLAT,
	PAGING_MODE_32,
	PAGING_MODE_PAE,
	PAGING_MODE_64,
};

struct vm_guest_paging {
	uint64_t	cr3;
	int		cpl;
	enum vm_cpu_mode cpu_mode;
	enum vm_paging_mode paging_mode;
};

/*
 * The data structures 'vie' and 'vie_op' are meant to be opaque to the
 * consumers of instruction decoding. The only reason why their contents
 * need to be exposed is because they are part of the 'vhm_request' structure.
 */
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
	int		base_register;		/* CPU_REG_xyz */
	int		index_register;		/* CPU_REG_xyz */
	int		segment_register;	/* CPU_REG_xyz */

	int64_t		displacement;		/* optional addr displacement */
	int64_t		immediate;		/* optional immediate operand */

	uint8_t		decoded;	/* set to 1 if successfully decoded */

	struct vie_op	op;			/* opcode description */
};

enum vm_exitcode {
	VM_EXITCODE_INOUT = 0,
	VM_EXITCODE_MMIO_EMUL,
	VM_EXITCODE_PCI_CFG,
	VM_EXITCODE_MAX
};

struct vm_inout {
	uint16_t	bytes:3;	/* 1 or 2 or 4 */
	uint16_t	in:1;
	uint16_t	string:1;
	uint16_t	rep:1;
	uint16_t	port;
	uint32_t	eax;		/* valid for out */
};

struct vm_inout_str {
	struct vm_inout	inout;		/* must be the first element */
	struct vm_guest_paging paging;
	uint64_t	rflags;
	uint64_t	cr0;
	uint64_t	index;
	uint64_t	count;		/* rep=1 (%rcx), rep=0 (1) */
	int		addrsize;
	enum cpu_reg_name seg_name;
	struct seg_desc seg_desc;
};

enum task_switch_reason {
	TSR_CALL,
	TSR_IRET,
	TSR_JMP,
	TSR_IDT_GATE,	/* task gate in IDT */
};

struct vm_task_switch {
	uint16_t	tsssel;		/* new TSS selector */
	int		ext;		/* task switch due to external event */
	uint32_t	errcode;
	int		errcode_valid;	/* push 'errcode' on the new stack */
	enum task_switch_reason reason;
	struct vm_guest_paging paging;
};

#endif	/* _VMM_H_ */
