/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vm0_boot.h>

    .section    entry, "ax"
    .align      8
    .code32

    .global     cpu_primary_save_32
cpu_primary_save_32:
    /* save context from 32bit mode */
    lea     vm0_boot_context, %eax
    sgdt    BOOT_CTX_GDT_OFFSET(%eax)
    sidt    BOOT_CTX_IDT_OFFSET(%eax)
    str     BOOT_CTX_TR_SEL_OFFSET(%eax)
    sldt    BOOT_CTX_LDT_SEL_OFFSET(%eax)
    movl    %cr0, %ecx
    movl    %ecx, BOOT_CTX_CR0_OFFSET(%eax)
    movl    %cr3, %ecx
    movl    %ecx, BOOT_CTX_CR3_OFFSET(%eax)
    movl    %cr4, %ecx
    movl    %ecx, BOOT_CTX_CR4_OFFSET(%eax)
    mov     %cs, %cx
    mov     %cx,  BOOT_CTX_CS_SEL_OFFSET(%eax)
    lar     %ecx, %ecx
    /* CS AR start from bit 8 */
    shr     $8, %ecx
    /* Clear Limit field, bit 8-11 */
    andl    $0x0000f0ff, %ecx
    mov     %ecx, BOOT_CTX_CS_AR_OFFSET(%eax)

    /* Save CS limit field */
    mov     %cs, %cx
    xor     %edx, %edx
    lsl     %ecx, %edx
    mov     %edx, BOOT_CTX_CS_LIMIT_OFFSET(%eax)

    mov     %es,  BOOT_CTX_ES_SEL_OFFSET(%eax)
    mov     %ss,  BOOT_CTX_SS_SEL_OFFSET(%eax)
    mov     %ds,  BOOT_CTX_DS_SEL_OFFSET(%eax)
    mov     %fs,  BOOT_CTX_FS_SEL_OFFSET(%eax)
    mov     %gs,  BOOT_CTX_GS_SEL_OFFSET(%eax)
    ret

    .code64
    .global     cpu_primary_save_64
cpu_primary_save_64:
    /* save context from 64bit mode */
    lea     vm0_boot_context(%rip), %r8
    sgdt    BOOT_CTX_GDT_OFFSET(%r8)
    sidt    BOOT_CTX_IDT_OFFSET(%r8)
    str     BOOT_CTX_TR_SEL_OFFSET(%r8)
    sldt    BOOT_CTX_LDT_SEL_OFFSET(%r8)
    mov     %cr0, %rcx
    mov     %rcx, BOOT_CTX_CR0_OFFSET(%r8)
    mov     %cr3, %rcx
    mov     %rcx, BOOT_CTX_CR3_OFFSET(%r8)
    mov     %cr4, %rcx
    mov     %rcx, BOOT_CTX_CR4_OFFSET(%r8)
    mov     %cs, %cx
    mov     %cx,  BOOT_CTX_CS_SEL_OFFSET(%r8)
    lar     %ecx, %ecx
    /* CS AR start from bit 8 */
    shr     $8, %ecx
    /* Clear Limit field, bit 8-11 */
    andl    $0x0000f0ff, %ecx
    mov     %ecx, BOOT_CTX_CS_AR_OFFSET(%r8)

    /* Save CS limit field */
    mov     %cs, %cx
    xor     %edx, %edx
    lsl     %ecx, %edx
    mov     %edx, BOOT_CTX_CS_LIMIT_OFFSET(%r8)

    mov     %es,  BOOT_CTX_ES_SEL_OFFSET(%r8)
    mov     %ss,  BOOT_CTX_SS_SEL_OFFSET(%r8)
    mov     %ds,  BOOT_CTX_DS_SEL_OFFSET(%r8)
    mov     %fs,  BOOT_CTX_FS_SEL_OFFSET(%r8)
    mov     %gs,  BOOT_CTX_GS_SEL_OFFSET(%r8)
    /* 0xc0000080 = MSR_IA32_EFER */
    movl    $0xc0000080, %ecx
    rdmsr
    movl    %eax, BOOT_CTX_EFER_LOW_OFFSET(%r8)
    movl    %edx, BOOT_CTX_EFER_HIGH_OFFSET(%r8)
    ret

    .text
    .align  8
    .global vm0_boot_context
vm0_boot_context:
    .rept   SIZE_OF_BOOT_CTX
    .byte 0x00
    .endr
