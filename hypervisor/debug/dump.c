/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/per_cpu.h>
#include <x86/mmu.h>
#include <x86/guest/vcpu.h>
#include <x86/vmx.h>
#include <x86/guest/vm.h>
#include <x86/init.h>
#include <logmsg.h>
#include <dump.h>
#include <reloc.h>

#define CALL_TRACE_HIERARCHY_MAX    20U
#define DUMP_STACK_SIZE 0x200U

static spinlock_t exception_spinlock = { .head = 0U, .tail = 0U, };
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
	uint16_t pcpu_id = pcpuid_from_vcpu(vcpu);

	pr_acrnlog("\n\n================================================");
	pr_acrnlog("================================\n\n");
	pr_acrnlog("Guest Registers:\r\n");
	pr_acrnlog("=	VM ID %d ==== vCPU ID %hu ===  pCPU ID %d ===="
			"world %d =============\r\n",
			vcpu->vm->vm_id, vcpu->vcpu_id, pcpu_id,
			vcpu->arch.cur_context);
	pr_acrnlog("=	RIP=0x%016lx  RSP=0x%016lx RFLAGS=0x%016lx\r\n",
			vcpu_get_rip(vcpu),
			vcpu_get_gpreg(vcpu, CPU_REG_RSP),
			vcpu_get_rflags(vcpu));
	pr_acrnlog("=	CR0=0x%016lx  CR2=0x%016lx  CR3=0x%016lx\r\n",
			vcpu_get_cr0(vcpu),
			vcpu_get_cr2(vcpu),
			exec_vmread(VMX_GUEST_CR3));
	pr_acrnlog("=	RAX=0x%016lx  RBX=0x%016lx  RCX=0x%016lx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RAX),
			vcpu_get_gpreg(vcpu, CPU_REG_RBX),
			vcpu_get_gpreg(vcpu, CPU_REG_RCX));
	pr_acrnlog("=	RDX=0x%016lx  RDI=0x%016lx  RSI=0x%016lx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RDX),
			vcpu_get_gpreg(vcpu, CPU_REG_RDI),
			vcpu_get_gpreg(vcpu, CPU_REG_RSI));
	pr_acrnlog("=	RBP=0x%016lx  R8=0x%016lx  R9=0x%016lx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RBP),
			vcpu_get_gpreg(vcpu, CPU_REG_R8),
			vcpu_get_gpreg(vcpu, CPU_REG_R9));
	pr_acrnlog("=	R10=0x%016lx  R11=0x%016lx  R12=0x%016lx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_R10),
			vcpu_get_gpreg(vcpu, CPU_REG_R11),
			vcpu_get_gpreg(vcpu, CPU_REG_R12));
	pr_acrnlog("=	R13=0x%016lx  R14=0x%016lx  R15=0x%016lx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_R13),
			vcpu_get_gpreg(vcpu, CPU_REG_R14),
			vcpu_get_gpreg(vcpu, CPU_REG_R15));
	pr_acrnlog("\r\n");
}

static void dump_guest_stack(struct acrn_vcpu *vcpu)
{
	uint32_t i;
	uint64_t tmp[DUMP_STACK_SIZE], fault_addr;
	uint32_t err_code = 0U;

	if (copy_from_gva(vcpu, tmp, vcpu_get_gpreg(vcpu, CPU_REG_RSP),
		DUMP_STACK_SIZE, &err_code, &fault_addr) < 0) {
		pr_acrnlog("\r\nUnabled to Copy Guest Stack:\r\n");
		return;
	}

	pr_acrnlog("\r\nGuest Stack:\r\n");
	pr_acrnlog("Dump stack for vcpu %hu, from gva 0x%016lx\r\n",
			vcpu->vcpu_id, vcpu_get_gpreg(vcpu, CPU_REG_RSP));
	for (i = 0U; i < (DUMP_STACK_SIZE >> 5U); i++) {
		pr_acrnlog("guest_rsp(0x%lx):  0x%016lx  0x%016lx 0x%016lx  0x%016lx\r\n",
				(vcpu_get_gpreg(vcpu, CPU_REG_RSP)+(i*32U)),
				tmp[i*4], tmp[(i*4)+1],
				tmp[(i*4)+2], tmp[(i*4)+3]);
	}
	pr_acrnlog("\r\n");
}

static void dump_guest_context(uint16_t pcpu_id)
{
	struct acrn_vcpu *vcpu = get_running_vcpu(pcpu_id);

	if (vcpu != NULL) {
		dump_guest_reg(vcpu);
		dump_guest_stack(vcpu);
	}
}

static void show_host_call_trace(uint64_t rsp, uint64_t rbp_arg, uint16_t pcpu_id)
{
	uint64_t rbp = rbp_arg, return_address;
	uint32_t i = 0U;
	uint32_t cb_hierarchy = 0U;
	uint64_t *sp = (uint64_t *)rsp;

	pr_acrnlog("\r\n delta = (actual_load_address - CONFIG_HV_RAM_START) = 0x%llx\r\n", get_hv_image_delta());
	pr_acrnlog("\r\nHost Stack: CPU_ID = %hu\r\n", pcpu_id);
	for (i = 0U; i < (DUMP_STACK_SIZE >> 5U); i++) {
		pr_acrnlog("addr(0x%lx)	0x%016lx  0x%016lx  0x%016lx  0x%016lx\r\n",
			(rsp + (i * 32U)), sp[i * 4U],
			sp[(i * 4U) + 1U], sp[(i * 4U) + 2U],
			sp[(i * 4U) + 3U]);
	}
	pr_acrnlog("\r\n");

	pr_acrnlog("Host Call Trace:\r\n");

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
		pr_acrnlog("----> 0x%016lx\r\n",
				*(uint64_t *)(rbp + sizeof(uint64_t)));
		rbp = *(uint64_t *)rbp;
		cb_hierarchy++;
	}
	pr_acrnlog("\r\n");
}

void asm_assert(int32_t line, const char *file, const char *txt)
{
	uint16_t pcpu_id = get_pcpu_id();
	uint64_t rsp = cpu_rsp_get();
	uint64_t rbp = cpu_rbp_get();

	pr_acrnlog("Assertion failed in file %s,line %d : %s",
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
	uint64_t cr2_val;

	pr_acrnlog("\n\n================================================");
	pr_acrnlog("================================\n=\n");
	if (ctx->vector < 0x20UL) {
		name = excp_names[ctx->vector];
		pr_acrnlog("= Unhandled exception: %d (%s)\n", ctx->vector, name);
	}

	/* Dump host register*/
	pr_acrnlog("\r\nHost Registers:\r\n");
	pr_acrnlog("=  Vector=0x%016llX  RIP=0x%016llX\n",
			ctx->vector, ctx->rip);
	pr_acrnlog("=     RAX=0x%016llX  RBX=0x%016llX  RCX=0x%016llX\n",
			ctx->gp_regs.rax, ctx->gp_regs.rbx, ctx->gp_regs.rcx);
	pr_acrnlog("=     RDX=0x%016llX  RDI=0x%016llX  RSI=0x%016llX\n",
			ctx->gp_regs.rdx, ctx->gp_regs.rdi, ctx->gp_regs.rsi);
	pr_acrnlog("=     RSP=0x%016llX  RBP=0x%016llX  RBX=0x%016llX\n",
			ctx->rsp, ctx->gp_regs.rbp, ctx->gp_regs.rbx);
	pr_acrnlog("=      R8=0x%016llX   R9=0x%016llX  R10=0x%016llX\n",
			ctx->gp_regs.r8, ctx->gp_regs.r9, ctx->gp_regs.r10);
	pr_acrnlog("=     R11=0x%016llX  R12=0x%016llX  R13=0x%016llX\n",
			ctx->gp_regs.r11, ctx->gp_regs.r12, ctx->gp_regs.r13);
	pr_acrnlog("=  RFLAGS=0x%016llX  R14=0x%016llX  R15=0x%016llX\n",
			ctx->rflags, ctx->gp_regs.r14, ctx->gp_regs.r15);
	pr_acrnlog("= ERRCODE=0x%016llX   CS=0x%016llX   SS=0x%016llX\n",
			ctx->error_code, ctx->cs, ctx->ss);
	asm volatile ("movq %%cr2, %0" : "=r" (cr2_val));
	pr_acrnlog("= CR2=0x%016llX", cr2_val);
	pr_acrnlog("\r\n");

	pr_acrnlog("=====================================================");
	pr_acrnlog("===========================\n");
}

void dump_exception(struct intr_excp_ctx *ctx, uint16_t pcpu_id)
{
	/* Obtain lock to ensure exception dump doesn't get corrupted */
	spinlock_obtain(&exception_spinlock);

	/* Dump host context */
	dump_intr_excp_frame(ctx);
	/* Show host stack */
	show_host_call_trace(ctx->gp_regs.rsp, ctx->gp_regs.rbp, pcpu_id);
	/* Dump guest context */
	dump_guest_context(pcpu_id);

	/* Save registers*/
	crash_ctx = ctx;
	cache_flush_invalidate_all();

	/* Release lock to let other CPUs handle exception */
	spinlock_release(&exception_spinlock);
}
