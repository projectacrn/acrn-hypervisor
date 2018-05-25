/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include        <cpu.h>
#include        <mmu.h>
#include        <gdt.h>
#include        <idt.h>
#include        <msr.h>

/* MULTIBOOT HEADER */
#define MULTIBOOT_HEADER_MAGIC 0x1badb002
#define MULTIBOOT_HEADER_FLAGS 0x00000002 /*flags bit 1 : enable mem_*, mmap_**/

    .section    multiboot_header, "a"

    .align     4

    /* header magic */
    .long   MULTIBOOT_HEADER_MAGIC
    /* header flags - flags bit 6 : enable mmap_* */
    .long   MULTIBOOT_HEADER_FLAGS
    /* header checksum = -(magic + flags) */
    .long   -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

    .section    entry, "ax"

    .align      8
    .code32

    .global     cpu_primary_start_32
cpu_primary_start_32:
    /* Disable interrupts */
    cli

    /* Clear direction flag */
    cld

    /* save eax and ebx */
    movl    %eax, %esp
    movl    %ebx, %ebp

    /* detect whether it is in long mode */
    movl    $MSR_IA32_EFER, %ecx
    rdmsr
    test     $MSR_IA32_EFER_LMA_BIT, %eax

    /* jump to 64bit entry if it is already in long mode */
    jne      cpu_primary_start_64

    /* save the MULTBOOT magic number & MBI */
    movl    %esp, (boot_regs)
    movl    %ebp, (boot_regs+4)

    /* Disable paging */
    mov     %cr0, %ebx
    andl    $~CR0_PG, %ebx
    mov     %ebx, %cr0

    /* Set DE, PAE, MCE and OS support bits in CR4 */
    movl    $(CR4_DE | CR4_PAE | CR4_MCE | CR4_OSFXSR | CR4_OSXMMEXCPT), %eax
    mov     %eax, %cr4

    /* Set CR3 to PML4 table address */
    movl    $cpu_boot32_page_tables_start, %edi
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
    mov     $cpu_primary32_gdt_ptr, %ebx
    lgdt    (%ebx)

    /* Perform a long jump based to start executing in 64-bit mode */
    ljmp    $HOST_GDT_RING0_CODE_SEL, $primary_start_long_mode

    .code64
    .org 0x200
    .global     cpu_primary_start_64
cpu_primary_start_64:
    /* save the MULTBOOT magic number & MBI */
    movl    %edi, (boot_regs)
    movl    %esi, (boot_regs+4)

primary_start_long_mode:

    /* Fix up the IDT desciptors */
    movl    $HOST_IDT, %edx
    movl    $HOST_IDT_ENTRIES, %ecx
.LFixUpIDT_Entries:
    xorl    %eax, %eax
    xchgl   %eax, 12(%edx)              /* Set rsvd bits to 0; eax now has
                                           high 32 of entry point */
    xchgl   %eax,  8(%edx)              /* Set bits 63..32 of entry point;
                                           eax now has low 32 of entry point */
    movw    %ax,    (%edx)              /* Set bits  0-15 of procedure entry
                                           point */
    shr     $16, %eax
    movw    %ax,   6(%edx)              /* Set bits 16-31 of entry point */
    addl    $X64_IDT_DESC_SIZE,%edx
    loop    .LFixUpIDT_Entries

    /* Load IDT */
    mov     $HOST_IDTR, %rcx
    lidtq   (%rcx)

    /* Load temportary GDT pointer value */
    mov     $cpu_primary32_gdt_ptr, %ebx
    lgdt    (%ebx)

    /* Replace CS with the correct value should we need it */
    mov     $HOST_GDT_RING0_CODE_SEL, %bx
    mov     %bx, jcs
    movabsq $jmpbuf, %rax
    rex.w ljmp  *(%rax)
.data
jmpbuf: .quad after
jcs:    .word 0
.text
after:

    /* Initialize temporary stack pointer */
    movq    $_ld_bss_end, %rsp
    add     $CPU_PAGE_SIZE,%rsp
    and     $(~(CPU_STACK_ALIGN - 1)),%rsp

    // load all selector registers with appropriate values
    xor     %edx, %edx
    lldt    %dx
    movl    $HOST_GDT_RING0_DATA_SEL,%eax
    mov     %eax,%ss  // Was 32bit POC Stack
    mov     %eax,%ds  // Was 32bit POC Data
    mov     %eax,%es  // Was 32bit POC Data
    mov     %edx,%fs  // Was 32bit POC Data
    mov     %edx,%gs  // Was 32bit POC CLS

   /* Push sp magic to top of stack for call trace */
   pushq   $SP_BOTTOM_MAGIC
   /* continue with chipset level initialization */
   call     bsp_boot_init

loop:
    jmp loop

    .align  4
    .global boot_regs
boot_regs:
    .long   0x00000000
    .long   0x00000000

    /* GDT table */
    .align  4
cpu_primary32_gdt:
    .quad   0x0000000000000000
    .quad   0x00af9b000000ffff
    .quad   0x00cf93000000ffff
cpu_primary32_gdt_end:

/* GDT pointer */
    .align  2
cpu_primary32_gdt_ptr:
    .short  (cpu_primary32_gdt_end - cpu_primary32_gdt) - 1
    .quad   cpu_primary32_gdt

/* PML4, PDPT, and PD tables initialized to map first 4 GBytes of memory */
    .align  CPU_PAGE_SIZE
    .global cpu_boot32_page_tables_start
cpu_boot32_page_tables_start:
    .quad   cpu_primary32_pdpt_addr + (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    .align  CPU_PAGE_SIZE
cpu_primary32_pdpt_addr:
    address = 0
    .rept   4
    .quad   cpu_primary32_pdt_addr + address + \
                                        (IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    address = address + CPU_PAGE_SIZE
    .endr
    .align  CPU_PAGE_SIZE
cpu_primary32_pdt_addr:
    address = 0
    .rept  2048
    .quad  address + (IA32E_PDPTE_PS_BIT | IA32E_COMM_P_BIT | IA32E_COMM_RW_BIT)
    address = address + 0x200000
    .endr

