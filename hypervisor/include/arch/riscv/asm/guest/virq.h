/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ARCH_RISCV_GUEST_VIRQ_H
#define ARCH_RISCV_GUEST_VIRQ_H

void vcpu_set_trap(struct acrn_vcpu *vcpu, struct riscv_vcpu_trap_info *trap);
void vcpu_queue_exception(struct acrn_vcpu *vcpu, struct riscv_vcpu_trap_info *trap);
int32_t vcpu_set_intr(struct acrn_vcpu *vcpu, uint32_t hwirq);
int32_t vcpu_clear_intr(struct acrn_vcpu *vcpu, uint32_t hwirq);
bool vcpu_inject_pending_intr(struct acrn_vcpu *vcpu);

#endif /* ARCH_RISCV_GUEST_VIRQ_H */
