/*
 * Copyright (C) 2021-2022 Intel Corporation.
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
int32_t read_vcbm(const struct acrn_vcpu *vcpu, uint32_t vmsr, uint64_t *rval);
int32_t write_vcbm(struct acrn_vcpu *vcpu, uint32_t vmsr, uint64_t val);
int32_t read_vclosid(const struct acrn_vcpu *vcpu, uint64_t *rval);
int32_t write_vclosid(struct acrn_vcpu *vcpu, uint64_t val);

#endif /* VCAT_H_ */

