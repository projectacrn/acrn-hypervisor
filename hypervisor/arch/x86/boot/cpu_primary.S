/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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

/* MULTIBOOT HEADER */
#define MULTIBOOT_HEADER_MAGIC 0x1badb002
#define MULTIBOOT_HEADER_FLAGS 0x00000002 /*flags bit 1 : enable mem_*, mmap_**/

    .extern cpu_primary_save32
    .extern cpu_primary_save64
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
    /* save the MULTBOOT magic number & MBI */
    movl    %eax, (boot_regs)
    movl    %ebx, (boot_regs+4)

    /* Save boot context from 32bit mode */
    call cpu_primary_save_32

    /* Disable interrupts */
    cli

    /* Clear direction flag */
    cld

    /* detect whether it is in long mode
     *
     *     0xc0000080 = MSR_IA32_EFER
     */
    movl    $0xc0000080, %ecx
    rdmsr
    /* 0x400 = MSR_IA32_EFER_LMA_BIT */
    test     $0x400, %eax

    /* jump to 64bit entry if it is already in long mode */
    jne      cpu_primary_start_64

    /* Disable paging */
    mov     %cr0, %ebx
    /* 0x7fffffff = ~CR0_PG */
    andl    $0x7fffffff, %ebx
    mov     %ebx, %cr0

    /* Set DE, PAE, MCE and OS support bits in CR4
     * 0x00000668 =
     *    (CR4_DE | CR4_PAE | CR4_MCE | CR4_OSFXSR | CR4_OSXMMEXCPT) */
    movl    $0x00000668, %eax
    mov     %eax, %cr4

    /* Set CR3 to PML4 table address */
    movl    $cpu_boot32_page_tables_start, %edi
    mov     %edi, %cr3

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
    mov     $cpu_primary32_gdt_ptr, %ebx
    lgdt    (%ebx)

    /* Perform a long jump based to start executing in 64-bit mode */
    /* 0x0008 = HOST_GDT_RING0_CODE_SEL */
    ljmp    $0x0008, $primary_start_long_mode

    .code64
    .org 0x200
    .global     cpu_primary_start_64
cpu_primary_start_64:
    /* save the MULTBOOT magic number & MBI */
    lea     boot_regs(%rip), %rax
    movl    %edi, (%rax)
    movl    %esi, 4(%rax)

    /* Save boot context from 64bit mode */
    call cpu_primary_save_64

primary_start_long_mode:

    /* Initialize temporary stack pointer */
    lea     ld_bss_end(%rip), %rsp
    /*0x1000 = PAGE_SIZE*/
    add     $0x1000,%rsp
    /* 16 = CPU_STACK_ALIGN */
    and     $(~(16 - 1)),%rsp

    /*
     * Fix up the .rela sections
     * Notes: this includes the fixup to IDT tables and temporary
     *     page tables
     */
    call relocate

    /* Load temportary GDT pointer value */
    lea     cpu_primary32_gdt_ptr(%rip), %rbx
    lgdt    (%ebx)

    /* Set the correct long jump address */
    lea     jmpbuf(%rip), %rax
    lea     after(%rip), %rbx
    mov     %rbx, (%rax)
    rex.w ljmp  *(%rax)
.data
jmpbuf: .quad 0
	/* 0x0008 = HOST_GDT_RING0_CODE_SEL */
        .word 0x0008
.text
after:
    /* 0x10 = HOST_GDT_RING0_DATA_SEL*/
    movl    $0x10,%eax
    mov     %eax,%ss  // Was 32bit POC Stack
    mov     %eax,%ds  // Was 32bit POC Data
    mov     %eax,%es  // Was 32bit POC Data
    mov     %eax,%fs  // Was 32bit POC Data
    mov     %eax,%gs  // Was 32bit POC CLS

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
    /*0x1000 = PAGE_SIZE*/
    .align  0x1000
    .global cpu_boot32_page_tables_start
cpu_boot32_page_tables_start:
    /* 0x3 = (PAGE_PRESENT | PAGE_RW) */
    .quad   cpu_primary32_pdpt_addr + 0x3
    /*0x1000 = PAGE_SIZE*/
    .align  0x1000
cpu_primary32_pdpt_addr:
    address = 0
    .rept   4
    /* 0x3 = (PAGE_PRESENT | PAGE_RW) */
    .quad   cpu_primary32_pdt_addr + address + 0x3
    /*0x1000 = PAGE_SIZE*/
    address = address + 0x1000
    .endr
    /*0x1000 = PAGE_SIZE*/
    .align  0x1000
cpu_primary32_pdt_addr:
    address = 0
    .rept  2048
    /* 0x83 = (PAGE_PSE | PAGE_PRESENT | PAGE_RW) */
    .quad  address + 0x83
    address = address + 0x200000
    .endr
