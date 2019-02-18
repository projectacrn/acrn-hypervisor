/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <util.h>
#include <boot_context.h>
#include <acrn_common.h>
#include <vcpu.h>
#include <trusty.h>

#define CAT__(A,B) A ## B
#define CAT_(A,B) CAT__(A,B)
#define CTASSERT(expr) \
typedef int32_t CAT_(CTA_DummyType,__LINE__)[(expr) ? 1 : -1]

/* Build time sanity checks to make sure hard-coded offset
*  is matching the actual offset!
*/
CTASSERT(BOOT_CTX_CR0_OFFSET       == offsetof(struct acrn_vcpu_regs, cr0));
CTASSERT(BOOT_CTX_CR3_OFFSET       == offsetof(struct acrn_vcpu_regs, cr3));
CTASSERT(BOOT_CTX_CR4_OFFSET       == offsetof(struct acrn_vcpu_regs, cr4));
CTASSERT(BOOT_CTX_IDT_OFFSET       == offsetof(struct acrn_vcpu_regs, idt));
CTASSERT(BOOT_CTX_GDT_OFFSET       == offsetof(struct acrn_vcpu_regs, gdt));
CTASSERT(BOOT_CTX_LDT_SEL_OFFSET   == offsetof(struct acrn_vcpu_regs, ldt_sel));
CTASSERT(BOOT_CTX_TR_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, tr_sel));
CTASSERT(BOOT_CTX_CS_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, cs_sel));
CTASSERT(BOOT_CTX_SS_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, ss_sel));
CTASSERT(BOOT_CTX_DS_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, ds_sel));
CTASSERT(BOOT_CTX_ES_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, es_sel));
CTASSERT(BOOT_CTX_FS_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, fs_sel));
CTASSERT(BOOT_CTX_GS_SEL_OFFSET    == offsetof(struct acrn_vcpu_regs, gs_sel));
CTASSERT(BOOT_CTX_CS_AR_OFFSET     == offsetof(struct acrn_vcpu_regs, cs_ar));
CTASSERT(BOOT_CTX_EFER_LOW_OFFSET  == offsetof(struct acrn_vcpu_regs, ia32_efer));
CTASSERT(BOOT_CTX_EFER_HIGH_OFFSET == offsetof(struct acrn_vcpu_regs, ia32_efer) + 4);

CTASSERT(CPU_CONTEXT_OFFSET_RAX    == offsetof(struct acrn_gp_regs, rax));
CTASSERT(CPU_CONTEXT_OFFSET_RBX    == offsetof(struct acrn_gp_regs, rbx));
CTASSERT(CPU_CONTEXT_OFFSET_RCX    == offsetof(struct acrn_gp_regs, rcx));
CTASSERT(CPU_CONTEXT_OFFSET_RDX    == offsetof(struct acrn_gp_regs, rdx));
CTASSERT(CPU_CONTEXT_OFFSET_RBP    == offsetof(struct acrn_gp_regs, rbp));
CTASSERT(CPU_CONTEXT_OFFSET_RSI    == offsetof(struct acrn_gp_regs, rsi));
CTASSERT(CPU_CONTEXT_OFFSET_RDI    == offsetof(struct acrn_gp_regs, rdi));
CTASSERT(CPU_CONTEXT_OFFSET_R8     == offsetof(struct acrn_gp_regs, r8));
CTASSERT(CPU_CONTEXT_OFFSET_R9     == offsetof(struct acrn_gp_regs, r9));
CTASSERT(CPU_CONTEXT_OFFSET_R10    == offsetof(struct acrn_gp_regs, r10));
CTASSERT(CPU_CONTEXT_OFFSET_R11    == offsetof(struct acrn_gp_regs, r11));
CTASSERT(CPU_CONTEXT_OFFSET_R12    == offsetof(struct acrn_gp_regs, r12));
CTASSERT(CPU_CONTEXT_OFFSET_R13    == offsetof(struct acrn_gp_regs, r13));
CTASSERT(CPU_CONTEXT_OFFSET_R14    == offsetof(struct acrn_gp_regs, r14));
CTASSERT(CPU_CONTEXT_OFFSET_R15    == offsetof(struct acrn_gp_regs, r15));
CTASSERT(CPU_CONTEXT_OFFSET_CR2    == offsetof(struct run_context, cr2));
CTASSERT(CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL
				   == offsetof(struct run_context, ia32_spec_ctrl));
CTASSERT(CPU_CONTEXT_OFFSET_RFLAGS == offsetof(struct run_context, rflags));
CTASSERT(CPU_CONTEXT_OFFSET_CR3 - CPU_CONTEXT_OFFSET_EXTCTX_START
				   == offsetof(struct ext_context, cr3));
CTASSERT(CPU_CONTEXT_OFFSET_IDTR - CPU_CONTEXT_OFFSET_EXTCTX_START
				   == offsetof(struct ext_context, idtr));
CTASSERT(CPU_CONTEXT_OFFSET_LDTR - CPU_CONTEXT_OFFSET_EXTCTX_START
				   == offsetof(struct ext_context, ldtr));
CTASSERT((sizeof(struct trusty_startup_param)
		+ sizeof(struct trusty_key_info)) < 0x1000U);
CTASSERT(NR_WORLD == 2);
