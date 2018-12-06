/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOOT_H
#define BOOT_H

#ifdef ASSEMBLER
#define BOOT_CTX_CR0_OFFSET         176
#define BOOT_CTX_CR3_OFFSET         192
#define BOOT_CTX_CR4_OFFSET         184
#define BOOT_CTX_IDT_OFFSET         144
#define BOOT_CTX_GDT_OFFSET         128
#define BOOT_CTX_LDT_SEL_OFFSET     280
#define BOOT_CTX_TR_SEL_OFFSET      282
#define BOOT_CTX_CS_SEL_OFFSET      268
#define BOOT_CTX_SS_SEL_OFFSET      270
#define BOOT_CTX_DS_SEL_OFFSET      272
#define BOOT_CTX_ES_SEL_OFFSET      274
#define BOOT_CTX_FS_SEL_OFFSET      276
#define BOOT_CTX_GS_SEL_OFFSET      278
#define BOOT_CTX_CS_AR_OFFSET       248
#define BOOT_CTX_CS_LIMIT_OFFSET    252
#define BOOT_CTX_EFER_LOW_OFFSET    200
#define BOOT_CTX_EFER_HIGH_OFFSET   204
#define SIZE_OF_BOOT_CTX            296
#else
#define BOOT_CTX_CR0_OFFSET         176U
#define BOOT_CTX_CR3_OFFSET         192U
#define BOOT_CTX_CR4_OFFSET         184U
#define BOOT_CTX_IDT_OFFSET         144U
#define BOOT_CTX_GDT_OFFSET         128U
#define BOOT_CTX_LDT_SEL_OFFSET     280U
#define BOOT_CTX_TR_SEL_OFFSET      282U
#define BOOT_CTX_CS_SEL_OFFSET      268U
#define BOOT_CTX_SS_SEL_OFFSET      270U
#define BOOT_CTX_DS_SEL_OFFSET      272U
#define BOOT_CTX_ES_SEL_OFFSET      274U
#define BOOT_CTX_FS_SEL_OFFSET      276U
#define BOOT_CTX_GS_SEL_OFFSET      278U
#define BOOT_CTX_CS_AR_OFFSET       248U
#define BOOT_CTX_CS_LIMIT_OFFSET    252U
#define BOOT_CTX_EFER_LOW_OFFSET    200U
#define BOOT_CTX_EFER_HIGH_OFFSET   204U
#define SIZE_OF_BOOT_CTX            296U
struct acrn_vcpu_regs;
extern struct acrn_vcpu_regs boot_context;
extern spinlock_t trampoline_spinlock;
extern uint8_t		ld_bss_start;
extern uint8_t		ld_bss_end;
#endif /* ASSEMBLER */
#endif /* BOOT_H */
