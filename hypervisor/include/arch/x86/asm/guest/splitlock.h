/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SPLITLOCK_H_
#define SPLITLOCK_H_

void vcpu_kick_splitlock_emulation(struct acrn_vcpu *cur_vcpu);
void vcpu_complete_splitlock_emulation(struct acrn_vcpu *cur_vcpu);
int32_t emulate_splitlock(struct acrn_vcpu *vcpu, uint32_t exception_vector, bool *queue_exception);

#endif /* SPLITLOCK_H_ */
