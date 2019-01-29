/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCR_H
#define VCR_H

/**
 * @file virtual_cr.h
 *
 * @brief public APIs for vCR operations
 */

void init_cr0_cr4_host_mask(void);

/**
 * @brief vCR from vcpu
 *
 * @defgroup vCR ACRN
 * @{
 */

/**
 * @brief get vcpu CR0 value
 *
 * Get & cache target vCPU's CR0 in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of CR0.
 */
uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu CR0 value
 *
 * Update target vCPU's CR0 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set CR0
 */
void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu CR2 value
 *
 * Get & cache target vCPU's CR2 in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of CR2.
 */
uint64_t vcpu_get_cr2(const struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu CR2 value
 *
 * Update target vCPU's CR2 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set CR2
 */
void vcpu_set_cr2(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu CR4 value
 *
 * Get & cache target vCPU's CR4 in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of CR4.
 */
uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu CR4 value
 *
 * Update target vCPU's CR4 in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set CR4
 */
void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @}
 */
/* End of vCR */

int32_t cr_access_vmexit_handler(struct acrn_vcpu *vcpu);

#endif /* VCR_H */
