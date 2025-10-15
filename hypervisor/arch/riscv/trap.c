/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/irq.h>
#include <asm/timer.h>
#include <asm/trap.h>
#include <cpu.h>
#include <logmsg.h>
#include <notify.h>
#include <softirq.h>
#include <debug/dump.h>


static void unexpected_trap_handler(const struct intr_excp_ctx *ctx)
{
	pr_err("Unexpected S mode trap 0x%lx\n", ctx->regs.cause);

	/* Halt the CPU */
	cpu_dead();
}

/* IRQ 1 - Supervisor software interrupt handler */
static void s_sw_irq_handler(void)
{
	cpu_csr_clear(sip, IP_IE_SSI);
	handle_smp_call();
}

static void dispatch_exception(const struct intr_excp_ctx *ctx)
{
	uint16_t pcpu_id = get_pcpu_id();

	/* Dump exception context */
	dump_exception(ctx, pcpu_id);

	/* Halt the CPU */
	cpu_dead();
}

/*
 * FIXME:
 * This logic need to be refined once irq multi-arch framework refine work
 * is done. Exception code 1(IPI), 5(timer) and 9(ext int) will be merged
 * into irq num namespace together, and keep a same entry in the exception
 * code table. Abstract PLIC/AIA as a irqchip that implement a get_irq API
 * to do the mapping between irq_num and PLIC source id or AIA's MSI.
 */
/**
 * TODO: add support for handler registration via request_irq() and
 *       further adoption of the common IRQ framework.
 */
static void dispatch_interrupt(const struct intr_excp_ctx *ctx)
{
	uint64_t trap_cause = ctx->regs.cause & (~TRAP_CAUSE_INTERRUPT_BITMASK);

	switch (trap_cause) {
	case TRAP_CAUSE_IRQ_S_SOFT:
		s_sw_irq_handler();
		break;
	case TRAP_CAUSE_IRQ_S_TIMER:
		timer_irq_handler();
		break;
	/* TODO: add support for external interrupt */
	default:
		unexpected_trap_handler(ctx);
		break;
	}

	do_softirq();
}

void dispatch_trap(const struct intr_excp_ctx *ctx)
{
	if ((ctx->regs.cause & TRAP_CAUSE_INTERRUPT_BITMASK) == 0UL) {
		dispatch_exception(ctx);
	} else {
		dispatch_interrupt(ctx);
	}
}
