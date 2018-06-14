/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <vcpu.h>

   .text
   .align   8
   .code64
   .extern    restore_msrs
   .extern    cpu_ctx
   .extern    load_gdtr_and_tr

   .global    __enter_s3
__enter_s3:
	movq %rax, CPU_CONTEXT_OFFSET_RAX + cpu_ctx(%rip)
	movq %rbx, CPU_CONTEXT_OFFSET_RBX + cpu_ctx(%rip)
	movq %rcx, CPU_CONTEXT_OFFSET_RCX + cpu_ctx(%rip)
	movq %rdx, CPU_CONTEXT_OFFSET_RDX + cpu_ctx(%rip)
	movq %rdi, CPU_CONTEXT_OFFSET_RDI + cpu_ctx(%rip)
	movq %rsi, CPU_CONTEXT_OFFSET_RSI + cpu_ctx(%rip)
	movq %rbp, CPU_CONTEXT_OFFSET_RBP + cpu_ctx(%rip)
	movq %rsp, CPU_CONTEXT_OFFSET_RSP + cpu_ctx(%rip)
	movq %r8,  CPU_CONTEXT_OFFSET_R8 + cpu_ctx(%rip)
	movq %r9,  CPU_CONTEXT_OFFSET_R9 + cpu_ctx(%rip)
	movq %r10, CPU_CONTEXT_OFFSET_R10 + cpu_ctx(%rip)
	movq %r11, CPU_CONTEXT_OFFSET_R11 + cpu_ctx(%rip)
	movq %r12, CPU_CONTEXT_OFFSET_R12 + cpu_ctx(%rip)
	movq %r13, CPU_CONTEXT_OFFSET_R13 + cpu_ctx(%rip)
	movq %r14, CPU_CONTEXT_OFFSET_R14 + cpu_ctx(%rip)
	movq %r15, CPU_CONTEXT_OFFSET_R15 + cpu_ctx(%rip)

	pushfq
	popq CPU_CONTEXT_OFFSET_RFLAGS + cpu_ctx(%rip)

	sidt CPU_CONTEXT_OFFSET_IDTR + cpu_ctx(%rip)
	sldt CPU_CONTEXT_OFFSET_LDTR + cpu_ctx(%rip)

	mov %cr0, %rax
	mov %rax, CPU_CONTEXT_OFFSET_CR0 + cpu_ctx(%rip)

	mov %cr3, %rax
	mov %rax, CPU_CONTEXT_OFFSET_CR3 + cpu_ctx(%rip)

	mov %cr4, %rax
	mov %rax, CPU_CONTEXT_OFFSET_CR4 + cpu_ctx(%rip)

	wbinvd

	/* Will add the function call to enter Sx here*/


/*
 * When system resume from S3, trampoline_start64 will
 * jump to restore_s3_context after setup temporary stack.
 */
.global restore_s3_context
restore_s3_context:
	mov CPU_CONTEXT_OFFSET_CR4 + cpu_ctx(%rip), %rax
	mov %rax, %cr4

	mov CPU_CONTEXT_OFFSET_CR3 + cpu_ctx(%rip), %rax
	mov %rax, %cr3

	mov CPU_CONTEXT_OFFSET_CR0 + cpu_ctx(%rip), %rax
	mov %rax, %cr0

	lidt CPU_CONTEXT_OFFSET_IDTR + cpu_ctx(%rip)
	lldt CPU_CONTEXT_OFFSET_LDTR + cpu_ctx(%rip)

	mov CPU_CONTEXT_OFFSET_SS + cpu_ctx(%rip), %ss
	mov CPU_CONTEXT_OFFSET_RSP + cpu_ctx(%rip), %rsp

	pushq CPU_CONTEXT_OFFSET_RFLAGS + cpu_ctx(%rip)
	popfq

	call load_gdtr_and_tr
	call restore_msrs

	movq CPU_CONTEXT_OFFSET_RAX + cpu_ctx(%rip), %rax
	movq CPU_CONTEXT_OFFSET_RBX + cpu_ctx(%rip), %rbx
	movq CPU_CONTEXT_OFFSET_RCX + cpu_ctx(%rip), %rcx
	movq CPU_CONTEXT_OFFSET_RDX + cpu_ctx(%rip), %rdx
	movq CPU_CONTEXT_OFFSET_RDI + cpu_ctx(%rip), %rdi
	movq CPU_CONTEXT_OFFSET_RSI + cpu_ctx(%rip), %rsi
	movq CPU_CONTEXT_OFFSET_RBP + cpu_ctx(%rip), %rbp
	movq CPU_CONTEXT_OFFSET_R8 + cpu_ctx(%rip), %r8
	movq CPU_CONTEXT_OFFSET_R9 + cpu_ctx(%rip), %r9
	movq CPU_CONTEXT_OFFSET_R10 + cpu_ctx(%rip), %r10
	movq CPU_CONTEXT_OFFSET_R11 + cpu_ctx(%rip), %r11
	movq CPU_CONTEXT_OFFSET_R12 + cpu_ctx(%rip), %r12
	movq CPU_CONTEXT_OFFSET_R13 + cpu_ctx(%rip), %r13
	movq CPU_CONTEXT_OFFSET_R14 + cpu_ctx(%rip), %r14
	movq CPU_CONTEXT_OFFSET_R15 + cpu_ctx(%rip), %r15

	retq
