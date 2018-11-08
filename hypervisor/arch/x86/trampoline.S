/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This is entry for AP startup and BSP S3 wakeup
 *
 * When system jump to trampoline_start16, the CPU is in x86 real
 * mode with no stack setup. CS:IP points to trampoline_start16.
 *
 * The CPU will be changed to long mode finally with temperay
 * page table and gdt in this file. Then jump to different C main
 * entry according to whether it's AP startup or BSP S3 resume.
 * The real page table and gdt will be setup in C main entry.
 */

#include        <spinlock.h>
#include        <gdt.h>
#include        <cpu.h>
#include        <mmu.h>
#include        <msr.h>

/* NOTE:
 *
 * MISRA C requires that all unsigned constants should have the suffix 'U'
 * (e.g. 0xffU), but the assembler may not accept such C-style constants. For
 * example, binutils 2.26 fails to compile assembly in that case. To work this
 * around, all unsigned constants must be explicitly spells out in assembly
 * with a comment tracking the original expression from which the magic
 * number is calculated. As an example:
 *
 *    /* 0x00000668 =
 *     *    (CR4_DE | CR4_PAE | CR4_MCE | CR4_OSFXSR | CR4_OSXMMEXCPT) *\/
 *    movl    $0x00000668, %eax
 *
 * Make sure that these numbers are updated accordingly if the definition of
 * the macros involved are changed.
 */

    .extern     cpu_secondary_init
    .extern     ld_bss_end
    .extern     HOST_GDTR

    .section     .trampoline_reset,"ax"

   .align   4
   .code16
   .global      trampoline_start16
   .org 0
trampoline_start16:

    /* Disable local interrupts */
    cli

    /*
     * There are two paths system could come here:
     *   - AP startup
     *     Silicon will set AP to real mode and setup CS:IP before
     *     jmp to trampoline_start16. And the IP is always 0 for sure.
     *   - BSP wakeup from S3
     *     Some bootloader (like ABL) doesn't guarante IP is set to
     *     zero before jump to trampoline_start16 after system resume
     *     from S3.
     *
     * To make trampoline code could work with all these cases, a far
     * jump is issued here as fixup. It will update the CS:IP according
     * to where the trampoline code is located.
     *
     * Here, we issue a far jump with "JMP ptr16:16" format (please refer
     * sdm vol2A - JMP instruction description). The jump target is set
     * to trampoline_fixup_target_addr. From trampoline_fixup_target_addr,
     * The CS has same value for both AP startup and BSP wakeup from S3.
     *
     * Because the limitation of real mode (can't access ip register
     * directly. So can't setup the trampoline_fixup_ip and
     * trampoline_fixup_cs), we have to update the trampoline_fixup_ip
     * and trampoline_fixup_cs when we preparing the trampoline code.
     *
     * Refer to preparing_trampoline() for fixup CS:IP setup
     */
    .byte 0xea	/* Opcode of "JMP ptr16:16" */
    .global trampoline_fixup_ip
trampoline_fixup_ip:
    .word   0	/* "EIP is intruction following JUMP instruction" */
    .global trampoline_fixup_cs
trampoline_fixup_cs:
    .word   0	/* CS */

    .global trampoline_fixup_target
trampoline_fixup_target:
    mov %cs, %ax
    mov %ax, %ds

    /* Set DE, PAE, MCE and OS support bits in CR4 */

    /* 0x00000668 =
     *    (CR4_DE | CR4_PAE | CR4_MCE | CR4_OSFXSR | CR4_OSXMMEXCPT) */
    movl    $0x00000668, %eax
    mov     %eax, %cr4

    /* Set CR3 to PML4 table address */

    movl    $cpu_boot_page_tables_ptr, %ebx
    mov     (%ebx), %eax
    mov     %eax, %cr3

    /* Set LME bit in EFER */

    /* 0xc0000080 = MSR_IA32_EFER */
    movl    $0xc0000080, %ecx
    rdmsr
    /* 0x00000100 = MSR_IA32_EFER_LME_BIT */
    orl     $0x00000100, %eax
    wrmsr

    /* Enable paging, protection, numeric error and co-processor
       monitoring in CR0 to enter long mode */

    mov     %cr0, %ebx
    /* 0x80000023 = (CR0_PG | CR0_PE | CR0_MP | CR0_NE) */
    orl     $0x80000023, %ebx
    mov     %ebx, %cr0

    /* Load temportary GDT pointer value */
    lgdt    (trampoline_gdt_ptr - trampoline_start16)

    /* Perform a long jump based to start executing in 64-bit mode */

    movl    $trampoline_start64_fixup, %ebx
    ljmpl   *(%ebx)

    .align 8
    .global trampoline_start64_fixup
trampoline_start64_fixup:
    .long   trampoline_start64
    /* 0x0008 = HOST_GDT_RING0_CODE_SEL */
    .word   0x0008

    .code64
trampoline_start64:

    /* Set up all other data segment registers */
    /* 0x0010 = HOST_GDT_RING0_DATA_SEL */
    movl    $0x0010, %eax
    mov     %eax, %ss
    mov     %eax, %ds
    mov     %eax, %es
    mov     %eax, %fs
    mov     %eax, %gs

    /* Obtain CPU spin-lock to serialize trampoline for different APs */
    movq    trampoline_spinlock_ptr(%rip), %rdi
    spinlock_obtain(%rdi)

    /* Initialize temporary stack pointer
       NOTE:  Using the PML4 memory (PDPT address is top of memory
              for the PML4 page) for the temporary stack
              as we are only using the very first entry in
              this page and the stack is growing down from
              the top of this page.  This stack is only
              used for a VERY short period of time, so
              this reuse of PML4 memory should be acceptable. */

    lea     trampoline_pdpt_addr(%rip), %rsp

    /* Jump to C entry */
    movq    main_entry(%rip), %rax
    jmp     *%rax


/* main entry */
    .align  8
    .global main_entry
main_entry:
    .quad   cpu_secondary_init /* default entry is AP start entry */

    .global trampoline_spinlock_ptr
trampoline_spinlock_ptr:
    .quad   trampoline_spinlock

/* GDT table */
    .align  4
trampoline_gdt:
    .quad   0x0000000000000000
    .quad   0x00af9b000000ffff
    .quad   0x00cf93000000ffff
trampoline_gdt_end:

/* GDT pointer */
    .align  2
    .global trampoline_gdt_ptr
trampoline_gdt_ptr:
    .short  (trampoline_gdt_end - trampoline_gdt) - 1
    .quad   trampoline_gdt

/* PML4, PDPT, and PD tables initialized to map first 4 GBytes of memory */
    .align  4
    .global cpu_boot_page_tables_ptr
cpu_boot_page_tables_ptr:
    .long cpu_boot_page_tables_start

    /*0x1000 = CPU_PAGE_SIZE*/
    .align  0x1000
    .global cpu_boot_page_tables_start
cpu_boot_page_tables_start:
    /* 0x3 = (PAGE_PRESENT | PAGE_RW) */
    .quad   trampoline_pdpt_addr + 0x3
    /*0x1000 = CPU_PAGE_SIZE*/
    .align  0x1000
    .global trampoline_pdpt_addr
trampoline_pdpt_addr:
    address = 0
    .rept   4
    /* 0x3 = (PAGE_PRESENT | PAGE_RW) */
    .quad   trampoline_pdt_addr + address + 0x3
    /*0x1000 = CPU_PAGE_SIZE*/
    address = address + 0x1000
    .endr
    /*0x1000 = CPU_PAGE_SIZE*/
    .align  0x1000
trampoline_pdt_addr:
    address = 0
    .rept  2048
    /* 0x83 = (PAGE_PSE | PAGE_PRESENT | PAGE_RW) */
    .quad  address + 0x83
    address = address + 0x200000
    .endr

    .end
