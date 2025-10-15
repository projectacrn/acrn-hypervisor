/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/irq.h>
#include <asm/page.h>

#define CALL_TRACE_HIERARCHY_MAX	0x10U
#define DUMP_STACK_SIZE			0x800U

void asm_assert(__unused int32_t line, __unused const char *file, __unused const char *txt) {}

static void show_host_call_trace(uint64_t stack_phy, uint64_t s0, uint16_t pcpu_id)
{
	uint32_t i = 0U;
	uint32_t cb_hierarchy = 0U;
	uint64_t *fp = (uint64_t *)s0;
	uint64_t *sp = (uint64_t *)stack_phy;
	uint64_t dump_size = min(roundup(stack_phy, PAGE_SIZE) - stack_phy, DUMP_STACK_SIZE);

	/* TODO: pritf the delta between the actual load address and the config load address here */
	//pr_acrnlog("\r\n delta = (actual_load_address - CONFIG_HV_RAM_START) = 0x%llx\r\n", get_hv_image_delta());
	pr_acrnlog("Host Stack: CPU_ID = %hu\r\n", pcpu_id);
	for (i = 0U; i < (dump_size >> 5U); i++) {
		pr_acrnlog("addr(0x%lx)	0x%016lx  0x%016lx  0x%016lx  0x%016lx\r\n",
			(stack_phy + (i * 32U)), sp[i * 4U],
			sp[(i * 4U) + 1U], sp[(i * 4U) + 2U],
			sp[(i * 4U) + 3U]);
	}
	pr_acrnlog("\r\n");

	pr_acrnlog("Host Call Trace:\r\n");

	/* if enable compiler option(no-omit-frame-pointer)  the stack layout
	 * should be like this when call a function for risc-v
	 *
	 *                  |                    |
	 *       fp + 16    |  last fp pointer   |
	 *       fp + 8     |  return address    |    push ra
	 *                  |                    |    sw fp s0
	 *
	 *       rsp        |                    |
	 *
	 *
	 *  if the address is invalid, it will cause hv page fault
	 *  then halt system */

	while (cb_hierarchy < CALL_TRACE_HIERARCHY_MAX) {
		pr_acrnlog("----> 0x%016lx\r\n", *(uint64_t *)(fp - 1));

		if (*fp == SP_BOTTOM_MAGIC) {
			break;
		}

		fp = (uint64_t *)(*(fp - 2));
		cb_hierarchy++;
	}

	pr_acrnlog("\r\n");
}

/* General purpose registers. */
// s2; s3; s4; s5; s6; s7; s8; s9; s10; s11;
// t3; t4; t5; t6;

void dump_intr_excp_frame(const struct intr_excp_ctx *ctx)
{
	pr_acrnlog("================================================");
	pr_acrnlog("================================\n");

	/* Dump host register*/
	pr_acrnlog("Host Registers:\r\n");
	pr_acrnlog("=  CAUSE=0x%016llX  EPC=0x%016llX\n",
			ctx->regs.cause, ctx->regs.epc);
	pr_acrnlog("=  TVAL=0x%016llX  STATUS=0x%016llX, SCRATCH=0x%016llX\n",
			ctx->regs.tval, ctx->regs.status, ctx->regs.scratch);

	pr_acrnlog("=     RA=0x%016llX  SP=0x%016llX  GP=0x%016llX, TP=0x%016llX\n",
			ctx->regs.ra, ctx->regs.sp, ctx->regs.gp, ctx->regs.tp);
	/* Temporary registers */
	pr_acrnlog("=     T0=0x%016llX  T1=0x%016llX  T2=0x%016llX\n",
			ctx->regs.t0, ctx->regs.t1, ctx->regs.t2);
	/* Callee-saved registers */
	pr_acrnlog("=     S0=0x%016llX  S1=0x%016llX\n",
			ctx->regs.s0, ctx->regs.s1);

	/* Argument registers */
	pr_acrnlog("=     A0=0x%016llX  A1=0x%016llX, A2=0x%016llX  A3=0x%016llX\n",
			ctx->regs.a0, ctx->regs.a1, ctx->regs.a2, ctx->regs.a3);
	pr_acrnlog("=     A4=0x%016llX  A5=0x%016llX, A6=0x%016llX  A7=0x%016llX\n",
			ctx->regs.a4, ctx->regs.a5, ctx->regs.a6, ctx->regs.a7);

	/* Callee-saved registers */
	pr_acrnlog("=     S2=0x%016llX  S3=0x%016llX, S4=0x%016llX  S5=0x%016llX\n",
			ctx->regs.s2, ctx->regs.s3, ctx->regs.s4, ctx->regs.s5);
	pr_acrnlog("=     S6=0x%016llX  S7=0x%016llX, S8=0x%016llX  S9=0x%016llX\n",
			ctx->regs.s6, ctx->regs.s7, ctx->regs.s8, ctx->regs.s9);
	pr_acrnlog("=     S10=0x%016llX  S11=0x%016llX\n",
			ctx->regs.s10, ctx->regs.s11);
	/* Temporary registers */
	pr_acrnlog("=     T3=0x%016llX  T4=0x%016llX  T5=0x%016llX, T6=0x%016llX\n",
			ctx->regs.t3, ctx->regs.t4, ctx->regs.t5, ctx->regs.t6);
	pr_acrnlog("\r\n");

	pr_acrnlog("=====================================================");
	pr_acrnlog("===========================\n");
}

void dump_exception(const struct intr_excp_ctx *ctx, uint16_t pcpu_id)
{
	/* Dump host context */
	dump_intr_excp_frame(ctx);

	/* Show host stack */
	show_host_call_trace(ctx->regs.sp, ctx->regs.s0, pcpu_id);
}
