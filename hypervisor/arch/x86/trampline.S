/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
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
trampline_start16:

    /* Disable local interrupts */

    cli
    mov %cs, %ax
    mov %ax, %ds

    /* Set DE, PAE, MCE and OS support bits in CR4 */

    movl    $(CR4_DE | CR4_PAE | CR4_MCE | CR4_OSFXSR | CR4_OSXMMEXCPT), %eax
    mov     %eax, %cr4

    /* Set CR3 to PML4 table address */

    movl    $CPU_Boot_Page_Tables_Start, %edi
    mov     %edi, %cr3

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

    data32 ljmp $HOST_GDT_RING0_CODE_SEL, $trampline_start64

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
    spinlock_obtain(trampline_spinlock)

    /* Initialize temporary stack pointer
       NOTE:  Using the PML4 memory (PDPT address is top of memory
              for the PML4 page) for the temporary stack
              as we are only using the very first entry in
              this page and the stack is growing down from
              the top of this page.  This stack is only
              used for a VERY short period of time, so
              this reuse of PML4 memory should be acceptable. */

    movq    $trampline_pdpt_addr, %rsp

    /* Push sp magic to top of stack for call trace */
    pushq   $SP_BOTTOM_MAGIC

    /* Jump to C entry */
    movq    main_entry(%rip), %rax
    jmp     %rax

trampline_error:    /* should never come here */
    jmp     trampline_error

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
trampline_gdt_ptr:
    .short  (trampline_gdt_end - trampline_gdt) - 1
    .quad   trampline_gdt

/* PML4, PDPT, and PD tables initialized to map first 4 GBytes of memory */

    .align  CPU_PAGE_SIZE
    .global CPU_Boot_Page_Tables_Start
CPU_Boot_Page_Tables_Start:
    .quad   trampline_pdpt_addr + (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    .align  CPU_PAGE_SIZE
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
