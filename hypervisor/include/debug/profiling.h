/*
 * Copyright (C) 2018 int32_tel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROFILING_H
#define PROFILING_H

#ifdef PROFILING_ON

#include <profiling_internal.h>

void profiling_vmenter_handler(struct acrn_vcpu *vcpu);
void profiling_vmexit_handler(struct acrn_vcpu *vcpu, uint64_t exit_reason);
void profiling_setup(void);

#else

static inline void profiling_vmenter_handler(__unused struct acrn_vcpu *vcpu) {}
static inline void profiling_vmexit_handler(__unused struct acrn_vcpu *vcpu,
	__unused uint64_t exit_reason) {}
static inline void profiling_setup(void) {}

#endif

#endif /* PROFILING_H */
