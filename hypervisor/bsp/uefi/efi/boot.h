/*
 * Copyright (c) 2011, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __ACRNBOOT_H__
#define __ACRNBOOT_H__

#include <bsp_cfg.h>

#define E820_RAM		1
#define E820_RESERVED		2
#define E820_ACPI		3
#define E820_NVS		4
#define E820_UNUSABLE		5

EFI_STATUS get_pe_section(CHAR8 *base, char *section, UINTN *vaddr, UINTN *size);

struct efi_info {
	UINT32 efi_loader_signature;
	UINT32 efi_systab;
	UINT32 efi_memdesc_size;
	UINT32 efi_memdesc_version;
	UINT32 efi_memmap;
	UINT32 efi_memmap_size;
	UINT32 efi_systab_hi;
	UINT32 efi_memmap_hi;
};

typedef struct {
	UINT16 limit;
	UINT64 *base;
} __attribute__((packed)) dt_addr_t;

struct e820_entry {
	UINT64 addr;		/* start of memory segment */
	UINT64 size;		/* size of memory segment */
	UINT32 type;		/* type of memory segment */
} __attribute__((packed));


struct efi_ctx {
	uint64_t rip;
	VOID *rsdp;
	dt_addr_t  gdt;
	dt_addr_t  idt;
	uint16_t   tr_sel;
	uint16_t   ldt_sel;
	uint64_t   cr0;
	uint64_t   cr3;
	uint64_t   cr4;
	uint64_t   rflags;
	uint16_t   cs_sel;
	uint32_t   cs_ar;
	uint16_t   es_sel;
	uint16_t   ss_sel;
	uint16_t   ds_sel;
	uint16_t   fs_sel;
	uint16_t   gs_sel;
	uint64_t   efer;
	uint64_t   rax;
	uint64_t   rbx;
	uint64_t   rcx;
	uint64_t   rdx;
	uint64_t   rdi;
	uint64_t   rsi;
	uint64_t   rsp;
	uint64_t   rbp;
	uint64_t   r8;
	uint64_t   r9;
	uint64_t   r10;
	uint64_t   r11;
	uint64_t   r12;
	uint64_t   r13;
	uint64_t   r14;
	uint64_t   r15;
}__attribute__((packed));

#endif

