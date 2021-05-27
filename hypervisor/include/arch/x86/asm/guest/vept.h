/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef VEPT_H
#define VEPT_H

#ifdef CONFIG_NVMX_ENABLED
/*
 * A descriptor to store info of nested EPT
 */
struct nept_desc {
	/*
	 * A shadow EPTP.
	 * The format is same with 'EPT pointer' in VMCS.
	 * Its PML4 address field is a HVA of the hypervisor.
	 */
	uint64_t shadow_eptp;
	/*
	 * An guest EPTP configured by L1 VM.
	 * The format is same with 'EPT pointer' in VMCS.
	 * Its PML4 address field is a GPA of the L1 VM.
	 */
	uint64_t guest_eptp;
	uint32_t ref_count;
};

void reserve_buffer_for_sept_pages(void);
void init_vept(void);
uint64_t get_shadow_eptp(uint64_t guest_eptp);
struct nept_desc *get_nept_desc(uint64_t guest_eptp);
void put_nept_desc(uint64_t guest_eptp);
int32_t invept_vmexit_handler(struct acrn_vcpu *vcpu);
#else
static inline void reserve_buffer_for_sept_pages(void) {};
#endif /* CONFIG_NVMX_ENABLED */
#endif /* VEPT_H */
