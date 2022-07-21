/*
 * Copyright (C) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/guest/vcpu.h>

void profiling_vmenter_handler(__unused struct acrn_vcpu *vcpu) {}
void profiling_pre_vmexit_handler(__unused struct acrn_vcpu *vcpu) {}
void profiling_post_vmexit_handler(__unused struct acrn_vcpu *vcpu) {}
void profiling_setup(void) {}

void sample_vmexit_end(__unused struct acrn_vcpu *vcpu) {}
void sample_vmexit_begin(__unused struct acrn_vcpu *vcpu) {}
