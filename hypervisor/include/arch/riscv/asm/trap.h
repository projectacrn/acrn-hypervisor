/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef RISCV_TRAP_H
#define RISCV_TRAP_H

#define TRAP_VECTOR_MODE_DIRECT        0U
#define TRAP_VECTOR_MODE_VECTORED      1U
/**
 * The Interrupt bit (most significant bit) in the scause register
 * is set if the trap was caused by an interrupt.
 */
#define TRAP_CAUSE_INTERRUPT_BITMASK	(1UL << 63U)

#define TRAP_CAUSE_EXC_MISALIGNED_FETCH		0x0
#define TRAP_CAUSE_EXC_FETCH_ACCESS		0x1
#define TRAP_CAUSE_EXC_ILLEGAL_INSTRUCTION	0x2
#define TRAP_CAUSE_EXC_BREAKPOINT		0x3
#define TRAP_CAUSE_EXC_MISALIGNED_LOAD		0x4
#define TRAP_CAUSE_EXC_LOAD_ACCESS		0x5
#define TRAP_CAUSE_EXC_MISALIGNED_STORE		0x6
#define TRAP_CAUSE_EXC_STORE_ACCESS		0x7
#define TRAP_CAUSE_EXC_USER_ECALL		0x8
#define TRAP_CAUSE_EXC_SUPERVISOR_ECALL		0x9
#define TRAP_CAUSE_EXC_VIRTUAL_SUPERVISOR_ECALL	0xa
#define TRAP_CAUSE_EXC_MACHINE_ECALL		0xb
#define TRAP_CAUSE_EXC_FETCH_PAGE_FAULT		0xc
#define TRAP_CAUSE_EXC_LOAD_PAGE_FAULT		0xd
#define TRAP_CAUSE_EXC_STORE_PAGE_FAULT		0xf
#define TRAP_CAUSE_EXC_DOUBLE_TRAP		0x10
#define TRAP_CAUSE_EXC_SW_CHECK_EXCP		0x12
#define TRAP_CAUSE_EXC_FETCH_GUEST_PAGE_FAULT	0x14
#define TRAP_CAUSE_EXC_LOAD_GUEST_PAGE_FAULT	0x15
#define TRAP_CAUSE_EXC_VIRTUAL_INST_FAULT	0x16
#define TRAP_CAUSE_EXC_STORE_GUEST_PAGE_FAULT	0x17

/* Trap Cause Codes - Interrupt */
/* Software Interrupt */
#define TRAP_CAUSE_IRQ_S_SOFT		1UL	/* Supervisor software interrupt */
#define TRAP_CAUSE_IRQ_VS_SOFT		2UL	/* Virtual supervisor software interrupt */
#define TRAP_CAUSE_IRQ_M_SOFT		3UL	/* Machine software interrupt */
/* Timer Interrupt */
#define TRAP_CAUSE_IRQ_S_TIMER		5UL	/* Supervisor timer interrupt */
#define TRAP_CAUSE_IRQ_VS_TIMER		6UL	/* Virtual supervisor timer interrupt */
#define TRAP_CAUSE_IRQ_M_TIMER		7UL	/* Machine timer interrupt */
/* External Interrupt */
#define TRAP_CAUSE_IRQ_S_EXT		9UL	/* Supervisor external interrupt */
#define TRAP_CAUSE_IRQ_VS_EXT		10UL	/* Virtual supervisor external interrupt */
#define TRAP_CAUSE_IRQ_M_EXT		11UL	/* Machine external interrupt */
#define TRAP_CAUSE_IRQ_S_GUEST_EXT	12UL	/* Supervisor guest external interrupt */
#define TRAP_CAUSE_IRQ_COUNTER_OVF	13UL	/* Reserved for counter-overflow interrupt */

/* Interrupt Pending/Enable registers flags */
/* Software Interrupt */
#define IP_IE_SSI			(1UL << TRAP_CAUSE_IRQ_S_SOFT)
#define IP_IE_VSSI			(1UL << TRAP_CAUSE_IRQ_VS_SOFT)
#define IP_IE_MSI			(1UL << TRAP_CAUSE_IRQ_M_SOFT)
/* Timer Interrupt */
#define IP_IE_STI			(1UL << TRAP_CAUSE_IRQ_S_TIMER)
#define IP_IE_VSTI			(1UL << TRAP_CAUSE_IRQ_VS_TIMER)
#define IP_IE_MTI			(1UL << TRAP_CAUSE_IRQ_M_TIMER)
/* External Interrupt */
#define IP_IE_SEI			(1UL << TRAP_CAUSE_IRQ_S_EXT)
#define IP_IE_VSEI			(1UL << TRAP_CAUSE_IRQ_VS_EXT)
#define IP_IE_MEI			(1UL << TRAP_CAUSE_IRQ_M_EXT)
#define IP_IE_SGEI			(1UL << TRAP_CAUSE_IRQ_S_GUEST_EXT)
#define IP_IE_LCOFI			(1UL << TRAP_CAUSE_IRQ_COUNTER_OVF)

#define HEDELEG_DEFAULT \
	((1UL << TRAP_CAUSE_EXC_MISALIGNED_FETCH) | \
	 (1UL << TRAP_CAUSE_EXC_MISALIGNED_LOAD) | \
	 (1UL << TRAP_CAUSE_EXC_MISALIGNED_STORE) | \
	 (1UL << TRAP_CAUSE_EXC_BREAKPOINT) | \
	 (1UL << TRAP_CAUSE_EXC_ILLEGAL_INSTRUCTION) | \
	 (1UL << TRAP_CAUSE_EXC_USER_ECALL) | \
	 (1UL << TRAP_CAUSE_EXC_FETCH_ACCESS) | \
	 (1UL << TRAP_CAUSE_EXC_LOAD_ACCESS) | \
	 (1UL << TRAP_CAUSE_EXC_STORE_ACCESS) | \
	 (1UL << TRAP_CAUSE_EXC_FETCH_PAGE_FAULT) | \
	 (1UL << TRAP_CAUSE_EXC_LOAD_PAGE_FAULT) | \
	 (1UL << TRAP_CAUSE_EXC_STORE_PAGE_FAULT))

#define HIDELEG_DEFAULT	\
	((1UL << TRAP_CAUSE_IRQ_VS_SOFT) | \
	 (1UL << TRAP_CAUSE_IRQ_VS_EXT) | \
	 (1UL << TRAP_CAUSE_IRQ_VS_TIMER))

#ifndef ASSEMBLER
#include <irq.h>

extern uint64_t strap_handler;
extern void vcpu_exit_return(void);

void dispatch_interrupt(const struct intr_excp_ctx *ctx);
void dispatch_trap(const struct intr_excp_ctx *ctx);
void s_sw_irq_handler(__unused uint32_t irq, __unused void *data);

#endif /* ASSEMBLER */

#endif /* RISCV_TRAP_H */
