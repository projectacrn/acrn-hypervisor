/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <init.h>

#define CALL_TRACE_HIERARCHY_MAX    20U
#define DUMP_STACK_SIZE 0x200U

/*
 * readable exception descriptors.
 */
static const char *const excp_names[32] = {
	[0] = "Divide Error",
	[1] = "RESERVED",
	[2] = "NMI",
	[3] = "Breakpoint",
	[4] = "Overflow",
	[5] = "BOUND range exceeded",
	[6] = "Invalid Opcode",
	[7] = "Device Not Available",
	[8] = "Double Fault",
	[9] = "Coprocessor Segment Overrun",
	[10] = "Invalid TSS",
	[11] = "Segment Not Present",
	[12] = "Stack Segment Fault",
	[13] = "General Protection",
	[14] = "Page Fault",
	[15] = "Intel Reserved",
	[16] = "x87 FPU Floating Point Error",
	[17] = "Alignment Check",
	[18] = "Machine Check",
	[19] = "SIMD Floating Point Exception",
	[20] = "Virtualization Exception",
	[21] = "Intel Reserved",
	[22] = "Intel Reserved",
	[23] = "Intel Reserved",
	[24] = "Intel Reserved",
	[25] = "Intel Reserved",
	[26] = "Intel Reserved",
	[27] = "Intel Reserved",
	[28] = "Intel Reserved",
	[29] = "Intel Reserved",
	[30] = "Intel Reserved",
	[31] = "Intel Reserved"
};

/*
 * Global variable for save registers on exception.
 * don't change crash_ctx to static.
 * crash_ctx is for offline analysis when system crashed, not for runtime usage.
 * as crash_ctx is only be set without being read, compiler will regard
 * crash_ctx as an useless variable if it is set to static, and will not
 * generate code for it.
 */
struct intr_excp_ctx *crash_ctx;

static void dump_guest_reg(struct acrn_vcpu *vcpu)
{
	printf("\n\n================================================");
	printf("================================\n\n");
	printf("Guest Registers:\r\n");
	printf("=	VM ID %d ==== vCPU ID %hu ===  pCPU ID %d ===="
			"world %d =============\r\n",
			vcpu->vm->vm_id, vcpu->vcpu_id, vcpu->pcpu_id,
			vcpu->arch.cur_context);
	printf("=	RIP=0x%016llx  RSP=0x%016llx "
			"RFLAGS=0x%016llx\r\n",
			vcpu_get_rip(vcpu),
			vcpu_get_gpreg(vcpu, CPU_REG_RSP),
			vcpu_get_rflags(vcpu));
	printf("=	CR0=0x%016llx  CR2=0x%016llx "
			" CR3=0x%016llx\r\n",
			vcpu_get_cr0(vcpu),
			vcpu_get_cr2(vcpu),
			exec_vmread(VMX_GUEST_CR3));
	printf("=	RAX=0x%016llx  RBX=0x%016llx  "
			"RCX=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RAX),
			vcpu_get_gpreg(vcpu, CPU_REG_RBX),
			vcpu_get_gpreg(vcpu, CPU_REG_RCX));
	printf("=	RDX=0x%016llx  RDI=0x%016llx  "
			"RSI=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RDX),
			vcpu_get_gpreg(vcpu, CPU_REG_RDI),
			vcpu_get_gpreg(vcpu, CPU_REG_RSI));
	printf("=	RBP=0x%016llx  R8=0x%016llx  "
			"R9=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RBP),
			vcpu_get_gpreg(vcpu, CPU_REG_R8),
			vcpu_get_gpreg(vcpu, CPU_REG_R9));
	printf("=	R10=0x%016llx  R11=0x%016llx  "
			"R12=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_R10),
			vcpu_get_gpreg(vcpu, CPU_REG_R11),
			vcpu_get_gpreg(vcpu, CPU_REG_R12));
	printf("=	R13=0x%016llx  R14=0x%016llx  "
			"R15=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_R13),
			vcpu_get_gpreg(vcpu, CPU_REG_R14),
			vcpu_get_gpreg(vcpu, CPU_REG_R15));
	printf("\r\n");
}

static void dump_guest_stack(struct acrn_vcpu *vcpu)
{
	uint32_t i;
	uint64_t tmp[DUMP_STACK_SIZE], fault_addr;
	uint32_t err_code = 0U;

	if (copy_from_gva(vcpu, tmp, vcpu_get_gpreg(vcpu, CPU_REG_RSP),
		DUMP_STACK_SIZE, &err_code, &fault_addr) < 0) {
		printf("\r\nUnabled to Copy Guest Stack:\r\n");
		return;
	}

	printf("\r\nGuest Stack:\r\n");
	printf("Dump stack for vcpu %hu, from gva 0x%016llx\r\n",
			vcpu->vcpu_id, vcpu_get_gpreg(vcpu, CPU_REG_RSP));
	for (i = 0U; i < (DUMP_STACK_SIZE >> 5U); i++) {
		printf("guest_rsp(0x%llx):  0x%016llx  0x%016llx  "
				"0x%016llx  0x%016llx\r\n",
				(vcpu_get_gpreg(vcpu, CPU_REG_RSP)+(i*32U)),
				tmp[i*4], tmp[(i*4)+1],
				tmp[(i*4)+2], tmp[(i*4)+3]);
	}
	printf("\r\n");
}

static void show_guest_call_trace(struct acrn_vcpu *vcpu)
{
	uint64_t bp;
	uint64_t count = 0UL;
	int32_t err;
	uint32_t err_code;

	bp = vcpu_get_gpreg(vcpu, CPU_REG_RBP);
	printf("Guest Call Trace: **************************************\r\n");
	printf("Maybe the call trace is not accurate, pls check stack!!\r\n");
	/* if enable compiler option(no-omit-frame-pointer)  the stack layout
	 * should be like this when call a function for x86_64
	 *
	 *                  |                    |
	 *       rbp+8      |  return address    |
	 *       rbp        |  rbp               |    push rbp
	 *                  |                    |    mov rsp rbp
	 *
	 *       rsp        |                    |
	 *
	 *  try to print out call trace,here can not check if the rbp is valid
	 *  if the address is invalid, it will cause hv page fault
	 *  then halt system */
	while ((count < CALL_TRACE_HIERARCHY_MAX) && (bp != 0UL)) {
		uint64_t parent_bp = 0UL, fault_addr;

		err_code = 0U;
		err = copy_from_gva(vcpu, &parent_bp, bp, sizeof(parent_bp),
			&err_code, &fault_addr);
		if (err < 0) {
			printf("\r\nUnabled to get Guest parent BP\r\n");
			return;
		}

		printf("BP_GVA(0x%016llx) RIP=0x%016llx\r\n", bp, parent_bp);
		/* Get previous rbp*/
		bp = parent_bp;
		count++;
	}
	printf("\r\n");
}

static void dump_guest_context(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu;

	vcpu = per_cpu(vcpu, pcpu_id);
	if (vcpu != NULL) {
		dump_guest_reg(vcpu);
		dump_guest_stack(vcpu);
		show_guest_call_trace(vcpu);
	}
}

static void show_host_call_trace(uint64_t rsp, uint64_t rbp_arg, uint16_t pcpu_id)
{
	uint64_t rbp = rbp_arg, return_address;
	uint32_t i = 0U;
	uint32_t cb_hierarchy = 0U;
	uint64_t *sp = (uint64_t *)rsp;

	printf("\r\nHost Stack: CPU_ID = %hu\r\n", pcpu_id);
	for (i = 0U; i < (DUMP_STACK_SIZE >> 5U); i++) {
		printf("addr(0x%llx)	0x%016llx  0x%016llx  0x%016llx  "
			"0x%016llx\r\n", (rsp + (i * 32U)), sp[i * 4U],
			sp[(i * 4U) + 1U], sp[(i * 4U) + 2U],
			sp[(i * 4U) + 3U]);
	}
	printf("\r\n");

	printf("Host Call Trace:\r\n");

	/* if enable compiler option(no-omit-frame-pointer)  the stack layout
	 * should be like this when call a function for x86_64
	 *
	 *                  |                    |
	 *       rbp+8      |  return address    |
	 *       rbp        |  rbp               |    push rbp
	 *                  |                    |    mov rsp rbp
	 *
	 *       rsp        |                    |
	 *
	 *
	 *  if the address is invalid, it will cause hv page fault
	 *  then halt system */
	while (cb_hierarchy < CALL_TRACE_HIERARCHY_MAX) {
		return_address = *(uint64_t *)(rbp + sizeof(uint64_t));
		if (return_address == SP_BOTTOM_MAGIC) {
			break;
		}
		printf("----> 0x%016llx\r\n",
				*(uint64_t *)(rbp + sizeof(uint64_t)));
		rbp = *(uint64_t *)rbp;
		cb_hierarchy++;
	}
	printf("\r\n");
}

void asm_assert(int32_t line, const char *file, const char *txt)
{
	uint16_t pcpu_id = get_cpu_id();
	uint64_t rsp = cpu_rsp_get();
	uint64_t rbp = cpu_rbp_get();

	printf("Assertion failed in file %s,line %d : %s",
			file, line, txt);
	show_host_call_trace(rsp, rbp, pcpu_id);
	dump_guest_context(pcpu_id);
	do {
		asm_pause();
	} while (1);
}

void dump_intr_excp_frame(const struct intr_excp_ctx *ctx)
{
	const char *name = "Not defined";

	printf("\n\n================================================");
	printf("================================\n=\n");
	if (ctx->vector < 0x20UL) {
		name = excp_names[ctx->vector];
		printf("= Unhandled exception: %d (%s)\n", ctx->vector, name);
	}

	/* Dump host register*/
	printf("\r\nHost Registers:\r\n");
	printf("=  Vector=0x%016llX  RIP=0x%016llX\n",
			ctx->vector, ctx->rip);
	printf("=     RAX=0x%016llX  RBX=0x%016llX  RCX=0x%016llX\n",
			ctx->gp_regs.rax, ctx->gp_regs.rbx, ctx->gp_regs.rcx);
	printf("=     RDX=0x%016llX  RDI=0x%016llX  RSI=0x%016llX\n",
			ctx->gp_regs.rdx, ctx->gp_regs.rdi, ctx->gp_regs.rsi);
	printf("=     RSP=0x%016llX  RBP=0x%016llX  RBX=0x%016llX\n",
			ctx->gp_regs.rsp, ctx->gp_regs.rbp, ctx->gp_regs.rbx);
	printf("=      R8=0x%016llX   R9=0x%016llX  R10=0x%016llX\n",
			ctx->gp_regs.r8, ctx->gp_regs.r9, ctx->gp_regs.r10);
	printf("=     R11=0x%016llX  R12=0x%016llX  R13=0x%016llX\n",
			ctx->gp_regs.r11, ctx->gp_regs.r12, ctx->gp_regs.r13);
	printf("=  RFLAGS=0x%016llX  R14=0x%016llX  R15=0x%016llX\n",
			ctx->rflags, ctx->gp_regs.r14, ctx->gp_regs.r15);
	printf("= ERRCODE=0x%016llX   CS=0x%016llX   SS=0x%016llX\n",
			ctx->error_code, ctx->cs, ctx->ss);
	printf("\r\n");

	printf("=====================================================");
	printf("===========================\n");
}

void dump_exception(struct intr_excp_ctx *ctx, uint16_t pcpu_id)
{
	/* Dump host context */
	dump_intr_excp_frame(ctx);
	/* Show host stack */
	show_host_call_trace(ctx->gp_regs.rsp, ctx->gp_regs.rbp, pcpu_id);
	/* Dump guest context */
	dump_guest_context(pcpu_id);

	/* Save registers*/
	crash_ctx = ctx;
	cache_flush_invalidate_all();
}
