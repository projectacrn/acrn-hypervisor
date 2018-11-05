/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ASSIGN_H
#define ASSIGN_H

#include <ptdev.h>

void ptdev_intx_ack(struct acrn_vm *vm, uint8_t virt_pin,
		enum ptdev_vpin_source vpin_src);
int ptdev_msix_remap(struct acrn_vm *vm, uint16_t virt_bdf,
		uint16_t entry_nr, struct ptdev_msi_info *info);
int ptdev_intx_pin_remap(struct acrn_vm *vm, uint8_t virt_pin,
		enum ptdev_vpin_source vpin_src);
int ptdev_add_intx_remapping(struct acrn_vm *vm, uint8_t virt_pin, uint8_t phys_pin,
		bool pic_pin);
void ptdev_remove_intx_remapping(const struct acrn_vm *vm, uint8_t virt_pin, bool pic_pin);
int ptdev_add_msix_remapping(struct acrn_vm *vm, uint16_t virt_bdf,
	uint16_t phys_bdf, uint32_t vector_count);
void ptdev_remove_msix_remapping(const struct acrn_vm *vm, uint16_t virt_bdf,
		uint32_t vector_count);

#endif /* ASSIGN_H */
