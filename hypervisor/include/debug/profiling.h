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

void profiling_vmenter_handler(struct acrn_vcpu *vcpu);
void profiling_vmexit_handler(struct acrn_vcpu *vcpu, uint64_t exit_reason);
void profiling_setup(void);

#endif /* PROFILING_H */
