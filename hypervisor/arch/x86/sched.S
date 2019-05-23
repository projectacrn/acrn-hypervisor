/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Function schedule() will finally call arch_switch_to here for x86 platform, which use
 * the pointer of previous & next thread_obj->host_sp as the input parameters (rdi & rsi).
 *
 * Function arch_switch_to will save rflags, rbx, rbp, r12~r15, and rdi in the previous
 * thread_obj's stack, then switch stack pointer(rsp) from previous to next thread_obj (saved
 * in thread_obj->host_sp) and restore above registers from next thread_obj's stack.
 * It make sure the execution context return to the same point of next thread_obj when it got
 * scheduled last time.
 */
    .text

   .code64
   .align       8
   .global      arch_switch_to
arch_switch_to:
	pushf
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushq %rdi
	movq %rsp, (%rdi)
	movq (%rsi), %rsp
	popq %rdi
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	popf
	retq
