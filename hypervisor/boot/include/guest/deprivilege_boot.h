/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef DEPRIVILEGE_BOOT_H

#define DEPRIVILEGE_BOOT_H

#include <vboot.h>

struct depri_boot_context {
	struct acrn_vcpu_regs vcpu_regs;
	void *rsdp;
	uint64_t ap_trampoline_buf;
} __packed;

const struct depri_boot_context *get_depri_boot_ctx(void);
const struct lapic_regs *get_depri_boot_lapic_regs(void);

struct vboot_operations* get_deprivilege_boot_ops(void);

#endif /* end of include guard: DEPRIVILEGE_BOOT_H */
