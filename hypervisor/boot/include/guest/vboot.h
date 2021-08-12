/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBOOT_H

#define VBOOT_H

int32_t init_vm_boot_info(struct acrn_vm *vm);
void load_sw_module(struct acrn_vm *vm, struct sw_module_info *sw_module);

#ifdef CONFIG_GUEST_KERNEL_BZIMAGE
int32_t bzimage_loader(struct acrn_vm *vm);
#endif
#ifdef CONFIG_GUEST_KERNEL_RAWIMAGE
int32_t rawimage_loader(struct acrn_vm *vm);
#endif
#ifdef CONFIG_GUEST_KERNEL_ELF
int32_t elf_loader(struct acrn_vm *vm);
#endif

#endif /* end of include guard: VBOOT_H */
