/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM0_BOOT_H
#define VM0_BOOT_H

#ifdef ASSEMBLER
#define BOOT_CTX_CR0_OFFSET         0
#define BOOT_CTX_CR3_OFFSET         8
#define BOOT_CTX_CR4_OFFSET         16
#define BOOT_CTX_IDT_OFFSET         24
#define BOOT_CTX_GDT_OFFSET         34
#define BOOT_CTX_LDT_SEL_OFFSET     44
#define BOOT_CTX_TR_SEL_OFFSET      46
#define BOOT_CTX_CS_SEL_OFFSET      48
#define BOOT_CTX_SS_SEL_OFFSET      50
#define BOOT_CTX_DS_SEL_OFFSET      52
#define BOOT_CTX_ES_SEL_OFFSET      54
#define BOOT_CTX_FS_SEL_OFFSET      56
#define BOOT_CTX_GS_SEL_OFFSET      58
#define BOOT_CTX_CS_AR_OFFSET       60
#define BOOT_CTX_EFER_LOW_OFFSET    64
#define BOOT_CTX_EFER_HIGH_OFFSET   68
#else
#include <gpr.h>
#define BOOT_CTX_CR0_OFFSET         0U
#define BOOT_CTX_CR3_OFFSET         8U
#define BOOT_CTX_CR4_OFFSET         16U
#define BOOT_CTX_IDT_OFFSET         24U
#define BOOT_CTX_GDT_OFFSET         34U
#define BOOT_CTX_LDT_SEL_OFFSET     44U
#define BOOT_CTX_TR_SEL_OFFSET      46U
#define BOOT_CTX_CS_SEL_OFFSET      48U
#define BOOT_CTX_SS_SEL_OFFSET      50U
#define BOOT_CTX_DS_SEL_OFFSET      52U
#define BOOT_CTX_ES_SEL_OFFSET      54U
#define BOOT_CTX_FS_SEL_OFFSET      56U
#define BOOT_CTX_GS_SEL_OFFSET      58U
#define BOOT_CTX_CS_AR_OFFSET       60U
#define BOOT_CTX_EFER_LOW_OFFSET    64U
#define BOOT_CTX_EFER_HIGH_OFFSET   68U

struct dt_addr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

struct boot_ctx {
	uint64_t   cr0;
	uint64_t   cr3;
	uint64_t   cr4;

	struct dt_addr  idt;
	struct dt_addr  gdt;
	uint16_t   ldt_sel;
	uint16_t   tr_sel;

	/* align the order to ext_context */
	uint16_t   cs_sel;
	uint16_t   ss_sel;
	uint16_t   ds_sel;
	uint16_t   es_sel;
	uint16_t   fs_sel;
	uint16_t   gs_sel;

	uint32_t   cs_ar;
	uint64_t   ia32_efer;
#ifdef CONFIG_EFI_STUB
	struct acrn_gp_regs gprs;
	uint64_t   rip;
	uint64_t   rflags;
	void *rsdp;
	void *ap_trampoline_buf;
#endif
}__attribute__((packed));

#ifdef CONFIG_EFI_STUB
void *get_rsdp_from_uefi(void);
void *get_ap_trampoline_buf(void);
#endif
#endif /* ASSEMBLER */
#endif /* VM0_BOOT_H */
