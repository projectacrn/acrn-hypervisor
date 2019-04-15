/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FIRMWARE_H

#define FIRMWARE_H

#define NUM_FIRMWARE_SUPPORTING 4U

struct acrn_vm;
struct firmware_operations {
	void (*init)(void);
	uint64_t (*get_ap_trampoline)(void);
	void *(*get_rsdp)(void);
	void (*init_irq)(void);
	int32_t (*init_vm_boot_info)(struct acrn_vm *vm);
};

struct firmware_candidates {
	const char name[20];
	size_t name_sz;
	struct firmware_operations *(*ops)(void);
};

void init_firmware_operations(void);
void init_firmware(void);
uint64_t firmware_get_ap_trampoline(void);
void *firmware_get_rsdp(void);
void firmware_init_irq(void);
int32_t firmware_init_vm_boot_info(struct acrn_vm *vm);

#ifndef CONFIG_CONSTANT_ACPI
void acpi_fixup(void);
#endif

#endif /* end of include guard: FIRMWARE_H */
