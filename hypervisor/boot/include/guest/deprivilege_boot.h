/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef FIRMWARE_UEFI_H

#define FIRMWARE_UEFI_H

#include <vboot.h>

struct uefi_context {
	struct acrn_vcpu_regs vcpu_regs;
	void *rsdp;
	void *ap_trampoline_buf;
} __packed;

const struct uefi_context *get_uefi_ctx(void);
const struct lapic_regs *get_uefi_lapic_regs(void);

struct firmware_operations* uefi_get_firmware_operations(void);
int32_t uefi_init_vm_boot_info(__unused struct acrn_vm *vm);

#endif /* end of include guard: FIRMWARE_UEFI_H */
