/*
 * Copyright (C) 2018 Intel Corporation.
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

#endif /* PROFILING_H */
