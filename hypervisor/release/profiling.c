/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

void profiling_vmenter_handler(__unused struct acrn_vcpu *vcpu) {}
void profiling_vmexit_handler(__unused struct acrn_vcpu *vcpu, __unused uint64_t exit_reason) {}
void profiling_setup(void) {}
