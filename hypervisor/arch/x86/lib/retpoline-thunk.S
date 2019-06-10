/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

.macro retpoline_thunk reg:req

.align 8
.global __x86_indirect_thunk_\reg
__x86_indirect_thunk_\reg:
    call 22f
11:
    pause
    jmp 11b
22:
    mov %\reg, (%rsp)
    ret
.endm

.section    .retpoline_thunk, "ax"

    retpoline_thunk rax
    retpoline_thunk rbx
    retpoline_thunk rcx
    retpoline_thunk rdx
    retpoline_thunk rdi
    retpoline_thunk rsi
    retpoline_thunk rbp
    retpoline_thunk r8
    retpoline_thunk r9
    retpoline_thunk r10
    retpoline_thunk r11
    retpoline_thunk r12
    retpoline_thunk r13
    retpoline_thunk r14
    retpoline_thunk r15
