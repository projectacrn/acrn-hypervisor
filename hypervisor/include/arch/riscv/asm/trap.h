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

#define TRAP_VECTOR_MODE_DIRECT		0U
#define TRAP_VECTOR_MODE_VECTORED	1U

/**
 * The Interrupt bit (most significant bit) in the scause register
 * is set if the trap was caused by an interrupt.
 */
#define TRAP_CAUSE_INTERRUPT_BITMASK	(1UL << 63U)

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

#ifndef ASSEMBLER
#include <irq.h>

extern uint64_t strap_handler;

void dispatch_trap(const struct intr_excp_ctx *ctx);
void s_sw_irq_handler(__unused uint32_t irq, __unused void *data);

#endif /* ASSEMBLER */

#endif /* RISCV_TRAP_H */
