/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBOOT_H

#define VBOOT_H

enum vboot_mode {
	DIRECT_BOOT_MODE,
	DEPRI_BOOT_MODE
};

struct vboot_operations {
	void (*init)(void);
	uint64_t (*get_ap_trampoline)(void);
	void *(*get_rsdp)(void);
	void (*init_irq)(void);
};

void init_vboot_operations(void);
void init_vboot(void);
void init_vboot_irq(void);
uint64_t get_ap_trampoline_buf(void);
void *get_rsdp_ptr(void);

enum vboot_mode get_sos_boot_mode(void);
int32_t parse_hv_cmdline(void);

#endif /* end of include guard: VBOOT_H */
