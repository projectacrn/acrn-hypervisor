/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef VEPT_H
#define VEPT_H

#ifdef CONFIG_NVMX_ENABLED
void reserve_buffer_for_sept_pages(void);
void init_vept(void);
int32_t invept_vmexit_handler(struct acrn_vcpu *vcpu);
#else
static inline void reserve_buffer_for_sept_pages(void) {};
#endif /* CONFIG_NVMX_ENABLED */
#endif /* VEPT_H */
