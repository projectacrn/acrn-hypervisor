/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

/*
 * readable exception descriptors.
 */
static const char *const excp_names[] = {
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

static void dump_guest_reg(struct vcpu *vcpu)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	printf("\n\n================================================");
	printf("================================\n\n");
	printf("Guest Registers:\r\n");
	printf("=	VM ID %d ==== vCPU ID %d ===  pCPU ID %d ===="
			"world %d =============\r\n",
			vcpu->vm->attr.id, vcpu->vcpu_id, vcpu->pcpu_id,
			vcpu->arch_vcpu.cur_context);
	printf("=	RIP=0x%016llx  RSP=0x%016llx "
			"RFLAGS=0x%016llx\r\n",
			cur_context->rip,
			cur_context->rsp,
			cur_context->rflags);
	printf("=	CR0=0x%016llx  CR2=0x%016llx "
			" CR3=0x%016llx\r\n",
			cur_context->cr0,
			cur_context->cr2,
			cur_context->cr3);
	printf("=	RAX=0x%016llx  RBX=0x%016llx  "
			"RCX=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.rax,
			cur_context->guest_cpu_regs.regs.rbx,
			cur_context->guest_cpu_regs.regs.rcx);
	printf("=	RDX=0x%016llx  RDI=0x%016llx  "
			"RSI=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.rdx,
			cur_context->guest_cpu_regs.regs.rdi,
			cur_context->guest_cpu_regs.regs.rsi);
	printf("=	RBP=0x%016llx  R8=0x%016llx  "
			"R9=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.rbp,
			cur_context->guest_cpu_regs.regs.r8,
			cur_context->guest_cpu_regs.regs.r9);
	printf("=	R10=0x%016llx  R11=0x%016llx  "
			"R12=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.r10,
			cur_context->guest_cpu_regs.regs.r11,
			cur_context->guest_cpu_regs.regs.r12);
	printf("=	R13=0x%016llx  R14=0x%016llx  "
			"R15=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.r13,
			cur_context->guest_cpu_regs.regs.r14,
			cur_context->guest_cpu_regs.regs.r15);
	printf("\r\n");
}

static void dump_guest_stack(struct vcpu *vcpu)
{
	uint64_t gpa;
	uint64_t hpa;
	uint32_t i;
	uint64_t *tmp;
	uint64_t page1_size;
	uint64_t page2_size;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	uint32_t err_code;
	int err;

	err_code = 0;
	err = gva2gpa(vcpu, cur_context->rsp, &gpa, &err_code);
	if (err) {
		printf("gva2gpa failed for guest rsp  0x%016llx\r\n",
			cur_context->rsp);
		return;
	}
	hpa = gpa2hpa(vcpu->vm, gpa);
	printf("\r\nGuest Stack:\r\n");
	printf("Dump stack for vcpu %d, from gva 0x%016llx ->"
			"gpa 0x%016llx -> hpa 0x%016llx \r\n",
			vcpu->vcpu_id, cur_context->rsp, gpa, hpa);
	/* Need check if cross 2 pages*/
	if (((cur_context->rsp % CPU_PAGE_SIZE) + DUMP_STACK_SIZE)
			<= CPU_PAGE_SIZE) {
		tmp = HPA2HVA(hpa);
		for (i = 0; i < DUMP_STACK_SIZE/32; i++) {
			printf("addr(0x%llx):  0x%016llx  0x%016llx  "
					"0x%016llx  0x%016llx\r\n", (hpa+i*32),
					tmp[i*4], tmp[i*4+1],
					tmp[i*4+2], tmp[i*4+3]);
		}

	} else {
		tmp = HPA2HVA(hpa);
		page1_size = CPU_PAGE_SIZE
			- (cur_context->rsp % CPU_PAGE_SIZE);
		for (i = 0; i < page1_size/32; i++) {
			printf("addr(0x%llx): 0x%016llx  0x%016llx  0x%016llx  "
					"0x%016llx\r\n", (hpa+i*32), tmp[i*4],
					tmp[i*4+1], tmp[i*4+2], tmp[i*4+3]);
		}
		err_code = 0;
		err = gva2gpa(vcpu, cur_context->rsp + page1_size, &gpa,
			&err_code);
		if (err) {
			printf("gva2gpa failed for guest rsp  0x%016llx\r\n",
				cur_context->rsp + page1_size);
			return;

		}
		hpa = gpa2hpa(vcpu->vm, gpa);
		printf("Dump stack for vcpu %d, from gva 0x%016llx ->"
				"gpa 0x%016llx -> hpa 0x%016llx \r\n",
				vcpu->vcpu_id, cur_context->rsp + page1_size,
				gpa, hpa);
		tmp = HPA2HVA(hpa);
		page2_size = DUMP_STACK_SIZE - page1_size;
		for (i = 0; i < page2_size/32; i++) {
			printf("addr(0x%llx): 0x%016llx  0x%016llx  0x%016llx  "
					"0x%016llx\r\n", (hpa+i*32), tmp[i*4],
					tmp[i*4+1], tmp[i*4+2], tmp[i*4+3]);
		}
	}

	printf("\r\n");
}

static void show_guest_call_trace(struct vcpu *vcpu)
{
	uint64_t gpa;
	uint64_t hpa;
	uint64_t *hva;
	uint64_t bp;
	uint64_t count = 0;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	int err;
	uint32_t err_code;

	bp =  cur_context->guest_cpu_regs.regs.rbp;
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
	while ((count++ < CALL_TRACE_HIERARCHY_MAX) && (bp != 0)) {
		err = gva2gpa(vcpu, bp, &gpa, &err_code);
		if (err) {
			printf("gva2gpa failed for guest bp 0x%016llx\r\n", bp);
			break;
		}
		hpa  = gpa2hpa(vcpu->vm, gpa);
		hva = HPA2HVA(hpa);
		printf("BP_GVA(0x%016llx)->BP_GPA(0x%016llx)"
			"->BP_HPA(0x%016llx) RIP=0x%016llx\r\n", bp, gpa, hpa,
			*(uint64_t *)((uint64_t)hva + sizeof(uint64_t)));
		/* Get previous rbp*/
		bp = *hva;
	}
	printf("\r\n");
}

static void dump_guest_context(uint32_t cpu_id)
{
	struct vcpu *vcpu;

	vcpu = per_cpu(vcpu, cpu_id);
	if (vcpu != NULL) {
		dump_guest_reg(vcpu);
		dump_guest_stack(vcpu);
		show_guest_call_trace(vcpu);
	}
}

static void show_host_call_trace(uint64_t rsp, uint64_t rbp, uint32_t cpu_id)
{
	int i = 0;
	int cb_hierarchy = 0;
	uint64_t *sp = (uint64_t *)rsp;

	printf("\r\nHost Stack: CPU_ID = %d\r\n", cpu_id);
	for (i = 0; i < DUMP_STACK_SIZE/32; i++) {
		printf("addr(0x%llx)	0x%016llx  0x%016llx  0x%016llx  "
				"0x%016llx\r\n", (rsp+i*32), sp[i*4], sp[i*4+1],
				sp[i*4+2], sp[i*4+3]);
	}
	printf("\r\n");

	printf("Host Call Trace:\r\n");
	if (rsp >
	(uint64_t)&per_cpu(stack, cpu_id)[STACK_SIZE - 1]
		|| rsp < (uint64_t)&per_cpu(stack, cpu_id)[0]) {
		return;
	}

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
	while ((rbp <=
	(uint64_t)&per_cpu(stack, cpu_id)[STACK_SIZE - 1])
		&& (rbp >= (uint64_t)&per_cpu(stack, cpu_id)[0])
		&& (cb_hierarchy++ < CALL_TRACE_HIERARCHY_MAX)) {
		printf("----> 0x%016llx\r\n",
				*(uint64_t *)(rbp + sizeof(uint64_t)));
		if (*(uint64_t *)(rbp + 2*sizeof(uint64_t))
				== SP_BOTTOM_MAGIC) {
			break;
		}
		rbp = *(uint64_t *)rbp;
	}
	printf("\r\n");
}

void __assert(uint32_t line, const char *file, char *txt)
{
	uint32_t cpu_id = get_cpu_id();
	uint64_t rsp = cpu_rsp_get();
	uint64_t rbp = cpu_rbp_get();

	pr_fatal("Assertion failed in file %s,line %u : %s",
			file, line, txt);
	show_host_call_trace(rsp, rbp, cpu_id);
	dump_guest_context(cpu_id);
	do {
		asm volatile ("pause" ::: "memory");
	} while (1);
}

void dump_intr_excp_frame(struct intr_excp_ctx *ctx)
{
	const char *name = "Not defined";

	printf("\n\n================================================");
	printf("================================\n=\n");
	if (ctx->vector < 0x20) {
		name = excp_names[ctx->vector];
		printf("= Unhandled exception: %d (%s)\n", ctx->vector, name);
	}

	/* Dump host register*/
	printf("\r\nHost Registers:\r\n");
	printf("=  Vector=0x%016llX  RIP=0x%016llX\n",
			ctx->vector, ctx->rip);
	printf("=     RAX=0x%016llX  RBX=0x%016llX  RCX=0x%016llX\n",
			ctx->rax, ctx->rbx, ctx->rcx);
	printf("=     RDX=0x%016llX  RDI=0x%016llX  RSI=0x%016llX\n",
			ctx->rdx, ctx->rdi, ctx->rsi);
	printf("=     RSP=0x%016llX  RBP=0x%016llX  RBX=0x%016llX\n",
			ctx->rsp, ctx->rbp, ctx->rbx);
	printf("=      R8=0x%016llX   R9=0x%016llX  R10=0x%016llX\n",
			ctx->r8, ctx->r9, ctx->r10);
	printf("=     R11=0x%016llX  R12=0x%016llX  R13=0x%016llX\n",
			ctx->r11, ctx->r12, ctx->r13);
	printf("=  RFLAGS=0x%016llX  R14=0x%016llX  R15=0x%016llX\n",
			ctx->rflags, ctx->r14, ctx->r15);
	printf("= ERRCODE=0x%016llX   CS=0x%016llX   SS=0x%016llX\n",
			ctx->error_code, ctx->cs, ctx->ss);
	printf("\r\n");

	printf("=====================================================");
	printf("===========================\n");
}

void dump_exception(struct intr_excp_ctx *ctx, uint32_t cpu_id)
{
	/* Dump host context */
	dump_intr_excp_frame(ctx);
	/* Show host stack */
	show_host_call_trace(ctx->rsp, ctx->rbp, cpu_id);
	/* Dump guest context */
	dump_guest_context(cpu_id);

}
