/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This is entry for AP startup and BSP S3 wakeup
 *
 * When system jump to trampline_start16, the CPU is in x86 real
 * mode with no stack setup. CS:IP points to trampline_start16.
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


    .extern     cpu_secondary_init
    .extern     cpu_logical_id
    .extern     _ld_bss_end
    .extern     HOST_GDTR

    .section     .trampline_reset,"ax"

   .align   4
   .code16
   .global      trampline_start16
   .org 0
trampline_start16:

    /* Disable local interrupts */
    cli

    /*
     * There are two paths system could come here:
     *   - AP startup
     *     Silicon will set AP to real mode and setup CS:IP before
     *     jmp to trampline_start16. And the IP is always 0 for sure.
     *   - BSP wakeup from S3
     *     Some bootloader (like ABL) doesn't guarante IP is set to
     *     zero before jump to trampline_start16 after system resume
     *     from S3.
     *
     * To make trampline code could work with all these cases, a far
     * jump is issued here as fixup. It will update the CS:IP according
     * to where the trampline code is located.
     *
     * Here, we issue a far jump with "JMP ptr16:16" format (please refer
     * sdm vol2A - JMP instruction description). The jump target is set
     * to trampline_fixup_target_addr. From trampline_fixup_target_addr,
     * The CS has same value for both AP startup and BSP wakeup from S3.
     *
     * Because the limitation of real mode (can't access ip register
     * directly. So can't setup the trampline_fixup_ip and
     * trampline_fixup_cs), we have to update the trampline_fixup_ip
     * and trampline_fixup_cs when we preparing the trampline code.
     *
     * Refer to preparing_trampline() for fixup CS:IP setup
     */
    .byte 0xea	/* Opcode of "JMP ptr16:16" */
    .global trampline_fixup_ip
trampline_fixup_ip:
    .word   0	/* "EIP is intruction following JUMP instruction" */
    .global trampline_fixup_cs
trampline_fixup_cs:
    .word   0	/* CS */

    .global trampline_fixup_target
trampline_fixup_target:
    mov %cs, %ax
    mov %ax, %ds

    /* Set DE, PAE, MCE and OS support bits in CR4 */

    movl    $(CR4_DE | CR4_PAE | CR4_MCE | CR4_OSFXSR | CR4_OSXMMEXCPT), %eax
    mov     %eax, %cr4

    /* Set CR3 to PML4 table address */

    movl    $CPU_Boot_Page_Tables_ptr, %ebx
    mov     (%ebx), %eax
    mov     %eax, %cr3

    /* Set LME bit in EFER */

    movl    $MSR_IA32_EFER, %ecx
    rdmsr
    orl     $MSR_IA32_EFER_LME_BIT, %eax
    wrmsr

    /* Enable paging, protection, numeric error and co-processor
       monitoring in CR0 to enter long mode */

    mov     %cr0, %ebx
    orl     $(CR0_PG | CR0_PE | CR0_MP | CR0_NE), %ebx
    mov     %ebx, %cr0

    /* Load temportary GDT pointer value */
    lgdt    (trampline_gdt_ptr - trampline_start16)

    /* Perform a long jump based to start executing in 64-bit mode */

    movl    $trampline_start64_fixup, %ebx
    ljmpl   *(%ebx)

    .align 8
    .global trampline_start64_fixup
trampline_start64_fixup:
    .long   trampline_start64
    .word   HOST_GDT_RING0_CODE_SEL

    .code64
trampline_start64:

    /* Set up all other data segment registers */

    movl    $HOST_GDT_RING0_DATA_SEL, %eax
    mov     %eax, %ss
    mov     %eax, %ds
    mov     %eax, %es
    mov     %eax, %fs
    mov     %eax, %gs

    /* Obtain CPU spin-lock to serialize trampline for different APs */
    mov     $trampline_spinlock, %rdi
    spinlock_obtain(%rdi)

    /* Initialize temporary stack pointer
       NOTE:  Using the PML4 memory (PDPT address is top of memory
              for the PML4 page) for the temporary stack
              as we are only using the very first entry in
              this page and the stack is growing down from
              the top of this page.  This stack is only
              used for a VERY short period of time, so
              this reuse of PML4 memory should be acceptable. */

    lea     trampline_pdpt_addr(%rip), %rsp

    /* Push sp magic to top of stack for call trace */
    pushq   $SP_BOTTOM_MAGIC

    /* Jump to C entry */
    movq    main_entry(%rip), %rax
    jmp     %rax


/* main entry */
    .align  8
    .global main_entry
main_entry:
    .quad   cpu_secondary_init /* default entry is AP start entry */

/* GDT table */
    .align  4
trampline_gdt:
    .quad   0x0000000000000000
    .quad   0x00af9b000000ffff
    .quad   0x00cf93000000ffff
trampline_gdt_end:

/* GDT pointer */
    .align  2
    .global trampline_gdt_ptr
trampline_gdt_ptr:
    .short  (trampline_gdt_end - trampline_gdt) - 1
    .quad   trampline_gdt

/* PML4, PDPT, and PD tables initialized to map first 4 GBytes of memory */
    .align  4
    .global CPU_Boot_Page_Tables_ptr
CPU_Boot_Page_Tables_ptr:
    .long CPU_Boot_Page_Tables_Start

    .align  CPU_PAGE_SIZE
    .global CPU_Boot_Page_Tables_Start
CPU_Boot_Page_Tables_Start:
    .quad   trampline_pdpt_addr + (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    .align  CPU_PAGE_SIZE
    .global trampline_pdpt_addr
trampline_pdpt_addr:
    address = 0
    .rept   4
    .quad   trampline_pdt_addr + address + \
                                        (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    address = address + CPU_PAGE_SIZE
    .endr
    .align  CPU_PAGE_SIZE
trampline_pdt_addr:
    address = 0
    .rept  2048
    .quad  address + (IA32E_PDPTE_PS_BIT | IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    address = address + 0x200000
    .endr

    .end
