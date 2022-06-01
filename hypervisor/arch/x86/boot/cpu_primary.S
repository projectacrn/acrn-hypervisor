/*
 * Copyright (C) 2018-2020 Intel Corporation.
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

#include <config.h>
#include <multiboot_std.h>

/* MULTIBOOT HEADER */
#define MULTIBOOT_HEADER_FLAGS	MULTIBOOT_HEADER_NEED_MEMINFO

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

#ifdef CONFIG_MULTIBOOT2
    .align     MULTIBOOT2_HEADER_ALIGN
mb2_header_start:
    /* Magic number indicating a Multiboot2 header. */
    .long   MULTIBOOT2_HEADER_MAGIC
    /* Architecture: i386. */
    .long   MULTIBOOT2_ARCHITECTURE_I386
    /* Multiboot2 header length. */
    .long   mb2_header_end - mb2_header_start
    /* Multiboot2 header checksum. */
    .long   -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCHITECTURE_I386 + (mb2_header_end - mb2_header_start))

    /* please be aware that each tag should be 8 bytes aligned */
    .align     MULTIBOOT2_TAG_ALIGN
    /*
     * Request infomation from boot loader, which is supposed to provide th relevant information
     * specified in the following tags to the image through the MBI if it is available
     */
info_req_tag_start:
    .short  MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST
    .short  0
    .long   info_req_tag_end - info_req_tag_start
    .long   MULTIBOOT2_TAG_TYPE_MMAP        /* memory map */
    .long   MULTIBOOT2_TAG_TYPE_MODULE      /* boot modules infomation */
    .long   MULTIBOOT2_TAG_TYPE_ACPI_NEW    /* a copy of RSDP as defined per ACPI 2.0 or later specification */
    .long   MULTIBOOT2_TAG_TYPE_EFI64       /* EFI system table, to be passed to guest Linux  */
    .long   MULTIBOOT2_TAG_TYPE_EFI_MMAP    /* EFI memory map, to be passed to guest Linux */
info_req_tag_end:

#ifdef CONFIG_RELOC
    .align  MULTIBOOT2_TAG_ALIGN
address_tag_start:
    .short  MULTIBOOT2_HEADER_TAG_ADDRESS
    .short  0
    .long   address_tag_end - address_tag_start
    .long   mb2_header_start  /* address corresponding to the beginning of the Multiboot2 header */
    .long   ld_ram_start     /* load_addr: load from the binary's beginning */
    /*
     * load_end_addr: this includes .bss so that boot loader could reserve the
     * memory that .bss occupies to avoid placing boot modules or other data in that area.
     *
     * However, the boot loader is supposed not to actually load the .bss section because
     * it's beyond the scope of acrn.bin
     */
    .long   ld_ram_end
    .long   0   /* bss_end_addr, don't ask boot loader to clear .bss */
address_tag_end:

    .align  MULTIBOOT2_TAG_ALIGN
entry_address_tag_start:
    .short  MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS
    .short  0
    .long   entry_address_tag_end - entry_address_tag_start
    .long   cpu_primary_start_32  /* The address to which the boot loader should jump to start hypervisor */
entry_address_tag_end:

    .align  MULTIBOOT2_TAG_ALIGN
relocatable_tag_start:
    .short  MULTIBOOT2_HEADER_TAG_RELOCATABLE
    .short  0
    .long   relocatable_tag_end - relocatable_tag_start
    .long   CONFIG_HV_RAM_START   /* min_addr */
    .long   0x80000000   /* max_addr */
    .long   0x200000     /* image alignment */
    .long   1            /* preference: lowest possible address */
relocatable_tag_end:
#endif	/* CONFIG_RELOC */

    .align     MULTIBOOT2_TAG_ALIGN
    .short  MULTIBOOT2_HEADER_TAG_END
    .short  0
    .long   8
mb2_header_end:
#endif	/* CONFIG_MULTIBOOT2 */

    /*
     * The page tables are aligned to 4KB, which implicitly aligns this section at
     * 4KB boundary. Put an extra .align here to explicitly state that regardless
     * the actual length of the multiboot header section, this section will be linked
     * at offset 0x1000 to the beginning of the target executable.
     */
    .align      0x1000
    .section    entry, "ax"
    .align      8
    .code32

    .global     cpu_primary_start_32
cpu_primary_start_32:

  /*
   * Calculate the relocation delta between where we were compiled to run
   * at and where we were actually loaded at.
   */
    call    0f
0:  pop     %esi
    sub     $0b, %esi

    /* save the MULTBOOT magic number & MBI */
    movl    %eax, boot_regs(%esi)
    movl    %ebx, (boot_regs+4)(%esi)

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
    jne      primary_start_long_mode

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

    /* fixup page table pointers with relocation delta */
    addl    %esi, cpu_primary32_pdpt_addr(%esi)
    addl    %esi, (cpu_primary32_pdpt_addr+8)(%esi)
    addl    %esi, (cpu_primary32_pdpt_addr+16)(%esi)
    addl    %esi, (cpu_primary32_pdpt_addr+24)(%esi)

    /* Set CR3 to PML4 table address */
    movl    $cpu_boot32_page_tables_start, %edi
    addl    %esi, %edi
    addl    %esi, (%edi)
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
    addl    %esi, %ebx
    addl    %esi, 2(%ebx)
    lgdt    (%ebx)

    /* Perform a long jump based to start executing in 64-bit mode */
    movl    $jmpbuf_32, %eax
    addl    %esi, %eax
    addl    %esi, (%eax)
    ljmp    *(%eax)

jmpbuf_32:
    .long   primary_start_long_mode
    /* 0x0008 = HOST_GDT_RING0_CODE_SEL */
    .word 0x0008

.code64
primary_start_long_mode:

    /* Initialize temporary stack pointer, size = 0x1000 */
    lea     stack_for_boot(%rip), %rsp
    /* 16 = CPU_STACK_ALIGN */
    and     $(~(16 - 1)),%rsp

    /*
     * Fix up the .rela sections
     * Notes: this includes the fixup to IDT tables and temporary
     *     page tables
     */
    call relocate

    call    0f
0:  pop     %rsi
    sub     $0b, %rsi    /* relocation delta */

    /* Load temportary GDT pointer value */
    lea     cpu_primary64_gdt_ptr(%rip), %rbx
    addq    %rsi, 2(%rbx)
    lgdt    (%ebx)

    /* Set the correct long jump address */
    lea     jmpbuf_64(%rip), %rax
    lea     after(%rip), %rbx
    mov     %rbx, (%rax)
    rex.w ljmp  *(%rax)
jmpbuf_64: .quad 0
	/* 0x0008 = HOST_GDT_RING0_CODE_SEL */
        .word 0x0008

after:
    /* 0x10 = HOST_GDT_RING0_DATA_SEL*/
    movl    $0x10,%eax
    mov     %eax,%ss  // Was 32bit POC Stack
    mov     %eax,%ds  // Was 32bit POC Data
    mov     %eax,%es  // Was 32bit POC Data
    mov     %eax,%fs  // Was 32bit POC Data
    mov     %eax,%gs  // Was 32bit POC CLS

   /* continue with chipset level initialization */
   call     init_primary_pcpu

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

cpu_primary64_gdt_ptr:
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
