/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#include <asm/csr.h>
#include <asm/trap.h>
#include <irq.h>
#include <vcpu.h>
#include <cpu.h>
#include <logmsg.h>
#include <notify.h>
#include <debug/dump.h>

#include <asm/guest/vcpu_exit.h>

static void unexpected_trap_handler(const struct intr_excp_ctx *ctx)
{
	pr_err("Unexpected S mode trap 0x%lx\n", ctx->regs.cause);

	/* Halt the CPU */
	cpu_dead();
}

/* IRQ 1 - Supervisor software interrupt handler */
void s_sw_irq_handler(__unused uint32_t irq, __unused void *data)
{
	cpu_csr_clear(CSR_SIP, IP_IE_SSI);
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

static void riscv_do_irq(const char *name, uint32_t src_id)
{
	uint32_t acrn_irq = riscv_domain_get_acrn_irq(name, src_id);

	if (riscv_is_valid_acrn_irq(acrn_irq)) {
		do_irq(acrn_irq);
	} else {
		pr_err("%s: Invalid IRQ %d\n", __func__, acrn_irq);
	}
}

void dispatch_interrupt(const struct intr_excp_ctx *ctx)
{
	uint64_t trap_cause = ctx->regs.cause & (~TRAP_CAUSE_INTERRUPT_BITMASK);

	switch (trap_cause) {
	case TRAP_CAUSE_IRQ_S_EXT:
		/**
		 * Handle TRAP_CAUSE_IRQ_S_EXT as a special case.
		 * Because recursion shall be avoided to comply with FuSa.
		 *
		 * FIXME:
		 * Abstract PLIC/AIA as a irqchip that implement a get_irq API
		 * to do the mapping between irq_num and PLIC source id or AIA's MSI.
		 *
		 * Pseudo-code:
		 * while ((src_id = get_pending_irq_from_chip()) != NONE) {
		 *     riscv_do_irq(RISCV_IRQD_PLIC, src_id);
		 * }
		 *
		 */
		unexpected_trap_handler(ctx);
		break;

	case TRAP_CAUSE_IRQ_S_SOFT:
		/* intentional fall-through */
	case TRAP_CAUSE_IRQ_S_TIMER:
		riscv_do_irq(RISCV_IRQD_CPU, trap_cause);
		break;

	default:
		unexpected_trap_handler(ctx);
		break;
	}
}

void dispatch_trap(const struct intr_excp_ctx *ctx)
{
	if ((ctx->regs.cause & TRAP_CAUSE_INTERRUPT_BITMASK) == 0UL) {
		dispatch_exception(ctx);
	} else {
		dispatch_interrupt(ctx);
	}
}
