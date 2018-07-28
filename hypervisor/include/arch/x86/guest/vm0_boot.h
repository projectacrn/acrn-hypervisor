/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM0_BOOT_H
#define VM0_BOOT_H

#include <gpr.h>

typedef struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) dt_addr_t;

struct boot_ctx {
	uint64_t   cr0;
	uint64_t   cr3;
	uint64_t   cr4;

	dt_addr_t  idt;
	dt_addr_t  gdt;
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
	struct cpu_gp_regs gprs;
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

#endif /* VM0_BOOT_H */
