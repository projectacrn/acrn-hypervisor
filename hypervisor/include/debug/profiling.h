/*
 * Copyright (C) 2018 int32_tel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PROFILING_H
#define PROFILING_H

#ifdef PROFILING_ON
#include <profiling_internal.h>
#endif

struct acrn_vcpu;

void profiling_vmenter_handler(struct acrn_vcpu *vcpu);
void profiling_pre_vmexit_handler(struct acrn_vcpu *vcpu);
void profiling_post_vmexit_handler(struct acrn_vcpu *vcpu);
void profiling_setup(void);

/* for vmexit sample */
void sample_vmexit_end(uint32_t basic_exit_reason, struct acrn_vcpu *vcpu);
void sample_vmexit_begin(uint32_t basic_exit_reason, struct acrn_vcpu *vcpu);

#endif /* PROFILING_H */
