/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCAT_H_
#define VCAT_H_

#include <asm/guest/vm.h>

bool is_l2_vcat_configured(const struct acrn_vm *vm);
bool is_l3_vcat_configured(const struct acrn_vm *vm);
uint16_t vcat_get_vcbm_len(const struct acrn_vm *vm, int res);
void init_vcat_msrs(struct acrn_vcpu *vcpu);
uint16_t vcat_get_num_vclosids(const struct acrn_vm *vm);
uint64_t vcat_pcbm_to_vcbm(const struct acrn_vm *vm, uint64_t pcbm, int res);

#endif /* VCAT_H_ */

