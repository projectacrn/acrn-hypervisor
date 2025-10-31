/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef RISCV_VCPU_PRIV_H
#define RISCV_VCPU_PRIV_H

/* arch internal API */
void vcpu_set_epc(struct acrn_vcpu *vcpu, uint64_t val);
void load_vcpu(struct acrn_vcpu *vcpu);
void unload_vcpu(struct acrn_vcpu *vcpu);
void flush_vcpu_context(struct acrn_vcpu *vcpu);
bool is_vcpu_context_updated(struct acrn_vcpu *vcpu);
void vcpu_mark_context_dirty(struct acrn_vcpu *vcpu);
#endif
