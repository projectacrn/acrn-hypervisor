/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VM0_BOOT_H
#define VM0_BOOT_H

#ifdef CONFIG_EFI_STUB
struct efi_context {
	struct acrn_vcpu_regs vcpu_regs;
	void *rsdp;
	void *ap_trampoline_buf;
} __packed;

int32_t efi_boot_init(void);
#endif

#endif /* VM0_BOOT_H */
