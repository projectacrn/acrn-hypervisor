/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _UEFI_H_
#define _UEFI_H_

struct efi_context {
	struct acrn_vcpu_regs vcpu_regs;
	void *rsdp;
	void *ap_trampoline_buf;
} __packed;

const struct efi_context *get_efi_ctx(void);
const struct lapic_regs *get_efi_lapic_regs(void);

#endif
