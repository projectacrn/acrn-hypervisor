/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef VEPT_H
#define VEPT_H

#ifdef CONFIG_NVMX_ENABLED

#define RESERVED_BITS(start, end) (((1UL << (end - start + 1)) - 1) << start)
#define IA32E_PML4E_RESERVED_BITS(phy_addr_width)	(RESERVED_BITS(3U, 7U) | RESERVED_BITS(phy_addr_width, 51U))
#define IA32E_PDPTE_RESERVED_BITS(phy_addr_width)	(RESERVED_BITS(3U, 6U) | RESERVED_BITS(phy_addr_width, 51U))
#define IA32E_PDPTE_LEAF_RESERVED_BITS(phy_addr_width)	(RESERVED_BITS(12U,29U)| RESERVED_BITS(phy_addr_width, 51U))
#define IA32E_PDE_RESERVED_BITS(phy_addr_width)		(RESERVED_BITS(3U, 6U) | RESERVED_BITS(phy_addr_width, 51U))
#define IA32E_PDE_LEAF_RESERVED_BITS(phy_addr_width)	(RESERVED_BITS(12U,20U)| RESERVED_BITS(phy_addr_width, 51U))
#define IA32E_PTE_RESERVED_BITS(phy_addr_width)		(RESERVED_BITS(phy_addr_width, 51U))

#define PAGING_ENTRY_SHIFT(lvl)		((IA32E_PT - (lvl)) * 9U + PTE_SHIFT)
#define PAGING_ENTRY_OFFSET(addr, lvl)	(((addr) >> PAGING_ENTRY_SHIFT(lvl)) & (PTRS_PER_PTE - 1UL))

/*
 * A descriptor to store info of nested EPT
 */
struct vept_desc {
	/*
	 * An guest EPTP configured by L1 VM.
	 * The format is same with 'EPT pointer' in VMCS.
	 * Its PML4 address field is a GPA of the L1 VM.
	 */
	uint64_t guest_eptp;
	/*
	 * A shadow EPTP.
	 * The format is same with 'EPT pointer' in VMCS.
	 * Its PML4 address field is a HVA of the hypervisor.
	 */
	uint64_t shadow_eptp;
	uint32_t ref_count;
};

void init_vept(void);
uint64_t get_shadow_eptp(uint64_t guest_eptp);
struct vept_desc *get_vept_desc(uint64_t guest_eptp);
void put_vept_desc(uint64_t guest_eptp);
bool handle_l2_ept_violation(struct acrn_vcpu *vcpu);
int32_t invept_vmexit_handler(struct acrn_vcpu *vcpu);
#else
static inline void init_vept(void) {};
#endif /* CONFIG_NVMX_ENABLED */
#endif /* VEPT_H */
