/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef UEFI_H
#define UEFI_H

typedef struct {
	uint16_t limit;
	uint64_t *base;
} __attribute__((packed)) dt_addr_t;

struct efi_ctx {
	uint64_t rip;
	void *rsdp;
	void *ap_trampline_buf;
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

void *get_rsdp_from_uefi(void);
void *get_ap_trampline_buf(void);

#endif /* UEFI_H*/
