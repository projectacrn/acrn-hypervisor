/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VBOOT_INFO_H

#define VBOOT_INFO_H

int32_t init_vm_boot_info(struct acrn_vm *vm);
void load_sw_module(struct acrn_vm *vm, struct sw_module_info *sw_module);

#ifdef CONFIG_GUEST_KERNEL_BZIMAGE
int32_t vm_bzimage_loader(struct acrn_vm *vm);
#endif
#ifdef CONFIG_GUEST_KERNEL_RAWIMAGE
int32_t vm_rawimage_loader(struct acrn_vm *vm);
#endif


#endif /* end of include guard: VBOOT_INFO_H */
