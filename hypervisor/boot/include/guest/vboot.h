/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBOOT_H

#define VBOOT_H

#define NUM_VBOOT_SUPPORTING 4U

struct acrn_vm;
struct vboot_operations {
	void (*init)(void);
	uint64_t (*get_ap_trampoline)(void);
	void *(*get_rsdp)(void);
	void (*init_irq)(void);
	int32_t (*init_vboot_info)(struct acrn_vm *vm);
};

struct vboot_candidates {
	const char name[20];
	size_t name_sz;
	struct vboot_operations *(*ops)(void);
};

void init_vboot_operations(void);
void init_vboot(void);
void init_vboot_irq(void);
int32_t init_vm_boot_info(struct acrn_vm *vm);
uint64_t get_ap_trampoline_buf(void);
void *get_rsdp_ptr(void);

int32_t parse_hv_cmdline(void);

#endif /* end of include guard: VBOOT_H */
