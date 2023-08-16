/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/mmu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/ept.h>
#include <asm/guest/vept.h>
#include <asm/guest/nested.h>

#define VETP_LOG_LEVEL			LOG_DEBUG
#define CONFIG_MAX_GUEST_EPT_NUM	(MAX_ACTIVE_VVMCS_NUM * MAX_VCPUS_PER_VM)
static struct vept_desc vept_desc_bucket[CONFIG_MAX_GUEST_EPT_NUM];
static spinlock_t vept_desc_bucket_lock;

/*
 * For simplicity, total platform RAM size is considered to calculate the
 * memory needed for shadow page tables. This is not an accurate upper bound.
 * This can satisfy typical use-cases where there is not a lot ofovercommitment
 * and sharing of memory between L2 VMs.
 *
 * Page table entry need 8 bytes to represent every 4K page frame.
 * Total number of bytes = (get_e820_ram_size() / PAGE_SIZE) * 8
 * Number of pages needed = Total number of bytes needed/PAGE_SIZE
 */
static uint64_t calc_sept_size(void)
{
	return (get_e820_ram_size() * 8UL) / PAGE_SIZE;
}

static uint64_t calc_sept_page_num(void)
{
	return calc_sept_size() / PAGE_SIZE;
}

static struct page_pool sept_page_pool;
static struct page *sept_pages;
static uint64_t *sept_page_bitmap;

/*
 * @brief Reserve space for SEPT pages from platform E820 table
 * 	  At moment, we only support nested VMX for Service VM.
 */
static void init_vept_pool(void)
{
	uint64_t page_base;

	page_base = e820_alloc_memory(calc_sept_size(), MEM_SIZE_MAX);

	set_paging_supervisor(page_base, calc_sept_size());

	sept_pages = (struct page *)page_base;
	sept_page_bitmap = (uint64_t *)e820_alloc_memory((calc_sept_page_num() / 64U), MEM_SIZE_MAX);
}

static bool is_present_ept_entry(uint64_t ept_entry)
{
	return ((ept_entry & EPT_RWX) != 0U);
}

static bool is_leaf_ept_entry(uint64_t ept_entry, enum _page_table_level pt_level)
{
	return (((ept_entry & PAGE_PSE) != 0U) || (pt_level == IA32E_PT));
}

/*
 * @brief Release all pages except the PML4E page of a shadow EPT
 */
static void free_sept_table(uint64_t *shadow_eptp)
{
	uint64_t *shadow_pml4e, *shadow_pdpte, *shadow_pde;
	uint64_t i, j, k;

	if (shadow_eptp) {
		for (i = 0UL; i < PTRS_PER_PML4E; i++) {
			shadow_pml4e = pml4e_offset(shadow_eptp, i << PML4E_SHIFT);
			if (!is_present_ept_entry(*shadow_pml4e)) {
				continue;
			}
			for (j = 0UL; j < PTRS_PER_PDPTE; j++) {
				shadow_pdpte = pdpte_offset(shadow_pml4e, j << PDPTE_SHIFT);
				if (!is_present_ept_entry(*shadow_pdpte) ||
				    is_leaf_ept_entry(*shadow_pdpte, IA32E_PDPT)) {
					continue;
				}
				for (k = 0UL; k < PTRS_PER_PDE; k++) {
					shadow_pde = pde_offset(shadow_pdpte, k << PDE_SHIFT);
					if (!is_present_ept_entry(*shadow_pde) ||
					    is_leaf_ept_entry(*shadow_pde, IA32E_PD)) {
						continue;
					}
					free_page(&sept_page_pool, (struct page *)((*shadow_pde) & EPT_ENTRY_PFN_MASK));
				}
				free_page(&sept_page_pool, (struct page *)((*shadow_pdpte) & EPT_ENTRY_PFN_MASK));
			}
			free_page(&sept_page_pool, (struct page *)((*shadow_pml4e) & EPT_ENTRY_PFN_MASK));
			*shadow_pml4e = 0UL;
		}
	}
}

/*
 * @brief Convert a guest EPTP to the associated vept_desc.
 * @return struct vept_desc * if existed.
 * @return NULL if non-existed.
 */
static struct vept_desc *find_vept_desc(uint64_t guest_eptp)
{
	uint32_t i;
	struct vept_desc *desc = NULL;

	if (guest_eptp) {
		spinlock_obtain(&vept_desc_bucket_lock);
		for (i = 0L; i < CONFIG_MAX_GUEST_EPT_NUM; i++) {
			/* Find an existed vept_desc of the guest EPTP */
			if (vept_desc_bucket[i].guest_eptp == guest_eptp) {
				desc = &vept_desc_bucket[i];
				break;
			}
		}
		spinlock_release(&vept_desc_bucket_lock);
	}

	return desc;
}

/*
 * @brief Convert a guest EPTP to a shadow EPTP.
 * @return 0 if non-existed.
 */
uint64_t get_shadow_eptp(uint64_t guest_eptp)
{
	struct vept_desc *desc = NULL;

	desc = find_vept_desc(guest_eptp);
	return (desc != NULL) ? hva2hpa((void *)desc->shadow_eptp) : 0UL;
}

/*
 * @brief Get a vept_desc to cache a guest EPTP
 *
 * If there is already an existed vept_desc associated with given guest_eptp,
 * increase its ref_count and return it. If there is not existed vept_desc
 * for guest_eptp, create one and initialize it.
 *
 * @return a vept_desc which associate the guest EPTP with a shadow EPTP
 */
struct vept_desc *get_vept_desc(uint64_t guest_eptp)
{
	uint32_t i;
	struct vept_desc *desc = NULL;

	if (guest_eptp != 0UL) {
		spinlock_obtain(&vept_desc_bucket_lock);
		for (i = 0L; i < CONFIG_MAX_GUEST_EPT_NUM; i++) {
			/* Find an existed vept_desc of the guest EPTP address bits */
			if (vept_desc_bucket[i].guest_eptp == guest_eptp) {
				desc = &vept_desc_bucket[i];
				desc->ref_count++;
				break;
			}
			/* Get the first empty vept_desc for the guest EPTP */
			if (!desc && (vept_desc_bucket[i].ref_count == 0UL)) {
				desc = &vept_desc_bucket[i];
			}
		}
		ASSERT(desc != NULL, "Get vept_desc failed!");

		/* A new vept_desc, initialize it */
		if (desc->shadow_eptp == 0UL) {
			desc->shadow_eptp = (uint64_t)alloc_page(&sept_page_pool) | (guest_eptp & ~PAGE_MASK);
			desc->guest_eptp = guest_eptp;
			desc->ref_count = 1UL;

			dev_dbg(VETP_LOG_LEVEL, "[%s], vept_desc[%llx] ref[%d] shadow_eptp[%llx] guest_eptp[%llx]",
					__func__, desc, desc->ref_count, desc->shadow_eptp, desc->guest_eptp);
		}

		spinlock_release(&vept_desc_bucket_lock);
	}

	return desc;
}

/*
 * @brief Put a vept_desc who associate with a guest_eptp
 *
 * If ref_count of the vept_desc, then release all resources used by it.
 */
void put_vept_desc(uint64_t guest_eptp)
{
	struct vept_desc *desc = NULL;

	if (guest_eptp != 0UL) {
		desc = find_vept_desc(guest_eptp);
		spinlock_obtain(&vept_desc_bucket_lock);
		if (desc) {
			desc->ref_count--;
			if (desc->ref_count == 0UL) {
				dev_dbg(VETP_LOG_LEVEL, "[%s], vept_desc[%llx] ref[%d] shadow_eptp[%llx] guest_eptp[%llx]",
						__func__, desc, desc->ref_count, desc->shadow_eptp, desc->guest_eptp);
				free_sept_table((void *)(desc->shadow_eptp & PAGE_MASK));
				free_page(&sept_page_pool, (struct page *)(desc->shadow_eptp & PAGE_MASK));
				/* Flush the hardware TLB */
				invept((void *)(desc->shadow_eptp & PAGE_MASK));
				desc->shadow_eptp = 0UL;
				desc->guest_eptp = 0UL;
			}
		}
		spinlock_release(&vept_desc_bucket_lock);
	}
}

static uint64_t get_leaf_entry(uint64_t gpa, uint64_t *eptp, enum _page_table_level *level)
{
	enum _page_table_level pt_level = IA32E_PML4;
	uint16_t offset;
	uint64_t ept_entry = 0UL;
	uint64_t *p_ept_entry = eptp;

	while (pt_level <= IA32E_PT) {
		offset = PAGING_ENTRY_OFFSET(gpa, pt_level);
		ept_entry = p_ept_entry[offset];

		if (is_present_ept_entry(ept_entry)) {
			if (is_leaf_ept_entry(ept_entry, pt_level)) {
				*level = pt_level;
				break;
			}
		} else {
			ept_entry = 0UL;
			pr_err("%s, GPA[%llx] is invalid!", __func__, gpa);
			break;
		}

		p_ept_entry = (uint64_t *)(ept_entry & EPT_ENTRY_PFN_MASK);
		pt_level += 1;
	}

	return ept_entry;
}

/**
 * @brief Shadow a guest EPT entry
 * @pre vcpu != NULL
 */
static uint64_t generate_shadow_ept_entry(struct acrn_vcpu *vcpu, uint64_t guest_ept_entry,
				    enum _page_table_level guest_ept_level)
{
	uint64_t shadow_ept_entry = 0UL;
	uint64_t ept_entry;
	enum _page_table_level ept_level;

	/*
	 * Create a shadow EPT entry
	 * We only support 4K page for guest EPT. So it's simple to create a shadow EPT entry
	 * for it. The rules are:
	 *   > Find the host EPT leaf entry of address in ept_entry[M-1:12], named as ept_entry
	 *   > Minimize the attribute bits (according to ept_entry and guest_ept_entry) and
	 *     set in shadow EPT entry shadow_ept_entry.
	 *   > Set the HPA of guest_ept_entry[M-1:12] to shadow_ept_entry.
	 */
	if (is_leaf_ept_entry(guest_ept_entry, guest_ept_level)) {
		ASSERT(guest_ept_level == IA32E_PT, "Only support 4K page for guest EPT!");
		ept_entry = get_leaf_entry((guest_ept_entry & EPT_ENTRY_PFN_MASK), get_eptp(vcpu->vm), &ept_level);
		if (ept_entry != 0UL) {
			/*
			 * TODO:
			 * Now, take guest EPT entry attributes directly. We need take care
			 * of memory type, permission bits, reserved bits when we merge EPT
			 * entry and guest EPT entry.
			 *
			 * Just keep the code skeleton here for extend.
			 */
			shadow_ept_entry = guest_ept_entry & ~EPT_ENTRY_PFN_MASK;

			/*
			 * Set the address.
			 * gpa2hpa() should be successful as ept_entry already be found.
			 */
			shadow_ept_entry |= gpa2hpa(vcpu->vm, (guest_ept_entry & EPT_ENTRY_PFN_MASK));
		}
	} else {
		/* Use a HPA of a new page in shadow EPT entry */
		shadow_ept_entry = guest_ept_entry & ~EPT_ENTRY_PFN_MASK;
		shadow_ept_entry |= hva2hpa((void *)alloc_page(&sept_page_pool)) & EPT_ENTRY_PFN_MASK;
	}

	return shadow_ept_entry;
}

/*
 * @brief Check misconfigurations on EPT entries
 *
 * SDM 28.2.3.1
 */
static bool is_ept_entry_misconfig(uint64_t ept_entry, enum _page_table_level pt_level)
{
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();
	uint8_t max_phy_addr_bits = cpu_info->phys_bits;
	bool is_misconfig = false;
	uint64_t reserved_bits = 0UL;
	uint8_t memory_type;

	/* Write w/o Read, misconfigured */
	is_misconfig = ((ept_entry & (EPT_RD | EPT_WR)) == EPT_WR);

	/* Execute-only is not supported */
	if (!pcpu_has_vmx_ept_vpid_cap(VMX_EPT_EXECUTE_ONLY)) {
		/* Execute w/o Read, misconfigured */
		is_misconfig = is_misconfig || ((ept_entry & (EPT_RD | EPT_EXE)) == EPT_EXE);
		/*
		 * TODO: With 'mode-based execute control for EPT' set,
		 * User-execute w/o Read, misconfigured
		 *	is_misconfig = is_misconfig || ((epte & (EPT_RD | EPT_XU)) == EPT_XU);
		 */
	}

	/* Reserved bits should be 0, else misconfigured */
	switch (pt_level) {
	case IA32E_PML4:
		reserved_bits = IA32E_PML4E_RESERVED_BITS(max_phy_addr_bits);
		break;
	case IA32E_PDPT:
		if (ept_entry & PAGE_PSE) {
			reserved_bits = IA32E_PDPTE_LEAF_RESERVED_BITS(max_phy_addr_bits);
		} else {
			reserved_bits = IA32E_PDPTE_RESERVED_BITS(max_phy_addr_bits);
		}
		break;
	case IA32E_PD:
		if (ept_entry & PAGE_PSE) {
			reserved_bits = IA32E_PDE_LEAF_RESERVED_BITS(max_phy_addr_bits);
		} else {
			reserved_bits = IA32E_PDE_RESERVED_BITS(max_phy_addr_bits);
		}
		break;
	case IA32E_PT:
		reserved_bits = IA32E_PTE_RESERVED_BITS(max_phy_addr_bits);
		break;
	default:
		break;
	}
	is_misconfig = is_misconfig || ((ept_entry & reserved_bits) != 0UL);

	/*
	 * SDM 28.2.6.2: The EPT memory type is specified in bits 5:3 of the last EPT
	 * paging-structure entry: 0 = UC; 1 = WC; 4 = WT; 5 = WP; and 6 = WB.
	 * Other values are reserved and cause EPT misconfiguration
	 */
	if (is_leaf_ept_entry(ept_entry, pt_level)) {
		memory_type = ept_entry & EPT_MT_MASK;
		is_misconfig = is_misconfig || ((memory_type != EPT_UNCACHED) &&
						(memory_type != EPT_WC) &&
						(memory_type != EPT_WT) &&
						(memory_type != EPT_WP) &&
						(memory_type != EPT_WB));
	}

	return is_misconfig;
}

static bool is_access_violation(uint64_t ept_entry)
{
	uint64_t exit_qual = exec_vmread(VMX_EXIT_QUALIFICATION);
	bool access_violation = false;

	if (/* Caused by data read */
	    (((exit_qual & 0x1UL) != 0UL) && ((ept_entry & EPT_RD) == 0)) ||
	    /* Caused by data write */
	    (((exit_qual & 0x2UL) != 0UL) && ((ept_entry & EPT_WR) == 0)) ||
	    /* Caused by instruction fetch */
	    (((exit_qual & 0x4UL) != 0UL) && ((ept_entry & EPT_EXE) == 0))) {
		access_violation = true;
	}

	return access_violation;
}

/**
 * @brief L2 VM EPT violation handler
 * @pre vcpu != NULL
 *
 * SDM: 28.2.3 EPT-Induced VM Exits
 *
 * Walk through guest EPT and fill the entries in shadow EPT
 */
bool handle_l2_ept_violation(struct acrn_vcpu *vcpu)
{
	uint64_t guest_eptp = vcpu->arch.nested.current_vvmcs->vmcs12.ept_pointer;
	struct vept_desc *desc = find_vept_desc(guest_eptp);
	uint64_t l2_ept_violation_gpa = exec_vmread(VMX_GUEST_PHYSICAL_ADDR_FULL);
	enum _page_table_level pt_level;
	uint64_t guest_ept_entry, shadow_ept_entry;
	uint64_t *p_guest_ept_page, *p_shadow_ept_page;
	uint16_t offset;
	bool is_l1_vmexit = true;

	ASSERT(desc != NULL, "Invalid shadow EPTP!");

	spinlock_obtain(&vept_desc_bucket_lock);
	stac();

	p_shadow_ept_page = (uint64_t *)(desc->shadow_eptp & PAGE_MASK);
	p_guest_ept_page = gpa2hva(vcpu->vm, desc->guest_eptp & PAGE_MASK);

	for (pt_level = IA32E_PML4; (p_guest_ept_page != NULL) && (pt_level <= IA32E_PT); pt_level++) {
		offset = PAGING_ENTRY_OFFSET(l2_ept_violation_gpa, pt_level);
		guest_ept_entry = p_guest_ept_page[offset];
		shadow_ept_entry = p_shadow_ept_page[offset];

		/*
		 * If guest EPT entry is non-exist, reflect EPT violation to L1 VM.
		 */
		if (!is_present_ept_entry(guest_ept_entry)) {
			break;
		}

		if (is_ept_entry_misconfig(guest_ept_entry, pt_level)) {
			/* Inject EPT_MISCONFIGURATION to L1 VM */
			exec_vmwrite(VMX_EXIT_REASON, VMX_EXIT_REASON_EPT_MISCONFIGURATION);
			break;
		}

		if (is_access_violation(guest_ept_entry)) {
			break;
		}

		/* Shadow EPT entry is non-exist, create it */
		if (!is_present_ept_entry(shadow_ept_entry)) {
			/* Create a shadow EPT entry */
			shadow_ept_entry = generate_shadow_ept_entry(vcpu, guest_ept_entry, pt_level);
			p_shadow_ept_page[offset] = shadow_ept_entry;
			if (shadow_ept_entry == 0UL) {
				/*
				 * TODO:
				 * For invalid GPA in guest EPT entries, now reflect the violation to L1 VM.
				 * Need to revisit this and evaluate if need to emulate the invalid GPA
				 * access of L2 in HV directly.
				 */
				break;
			}
		}

		/*
		 * SDM 28.3.3.4 Guidelines for Use of the INVEPT Instruction:
		 * Software may use the INVEPT instruction after modifying a present EPT
		 * paging-structure entry (see Section 28.2.2) to change any of the
		 * privilege bits 2:0 from 0 to 1. Failure to do so may cause an EPT
		 * violation that would not otherwise occur. Because an EPT violation
		 * invalidates any mappings that would be used by the access that caused
		 * the EPT violation (see Section 28.3.3.1), an EPT violation will not
		 * recur if the original access is performed again, even if the INVEPT
		 * instruction is not executed.
		 *
		 * If access bits of guest EPT entry is added after shadow EPT entry setup,
		 * guest VM may not call INVEPT. Sync it here directly.
		 */
		shadow_ept_entry = (shadow_ept_entry & ~EPT_RWX) | (guest_ept_entry & EPT_RWX);
		p_shadow_ept_page[offset] = shadow_ept_entry;

		/* Shadow EPT entry exists */
		if (is_leaf_ept_entry(guest_ept_entry, pt_level)) {
			/* Shadow EPT is set up, let L2 VM re-execute the instruction. */
			if ((exec_vmread32(VMX_IDT_VEC_INFO_FIELD) & VMX_INT_INFO_VALID) == 0U) {
				is_l1_vmexit = false;
			}
			break;
		} else {
			/* Set up next level EPT entries. */
			p_shadow_ept_page = hpa2hva(shadow_ept_entry & EPT_ENTRY_PFN_MASK);
			p_guest_ept_page = gpa2hva(vcpu->vm, guest_ept_entry & EPT_ENTRY_PFN_MASK);
		}
	}

	clac();
	spinlock_release(&vept_desc_bucket_lock);

	return is_l1_vmexit;
}

/**
 * @pre vcpu != NULL
 */
int32_t invept_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t i;
	struct vept_desc *desc;
	struct invept_desc operand_gla_ept;
	uint64_t type, ept_cap_vmsr;

	if (check_vmx_permission(vcpu)) {
		ept_cap_vmsr = vcpu_get_guest_msr(vcpu, MSR_IA32_VMX_EPT_VPID_CAP);
		type = get_invvpid_ept_operands(vcpu, (void *)&operand_gla_ept, sizeof(operand_gla_ept));
		if (gpa2hpa(vcpu->vm, operand_gla_ept.eptp) == INVALID_HPA) {
			nested_vmx_result(VMfailValid, VMXERR_INVEPT_INVVPID_INVALID_OPERAND);
		} else if (type == 1 && (ept_cap_vmsr & VMX_EPT_INVEPT_SINGLE_CONTEXT) != 0UL) {
			/* Single-context invalidation */
			/* Find corresponding vept_desc of the invalidated EPTP */
			desc = get_vept_desc(operand_gla_ept.eptp);
			if (desc) {
				spinlock_obtain(&vept_desc_bucket_lock);
				if (desc->shadow_eptp != 0UL) {
					/*
					 * Since ACRN does not know which paging entries are changed,
					 * Remove all the shadow EPT entries that ACRN created for L2 VM
					 */
					free_sept_table((void *)(desc->shadow_eptp & PAGE_MASK));
					invept((void *)(desc->shadow_eptp & PAGE_MASK));
				}
				spinlock_release(&vept_desc_bucket_lock);
				put_vept_desc(operand_gla_ept.eptp);
			}
			nested_vmx_result(VMsucceed, 0);
		} else if ((type == 2) && (ept_cap_vmsr & VMX_EPT_INVEPT_GLOBAL_CONTEXT) != 0UL) {
			/* Global invalidation */
			spinlock_obtain(&vept_desc_bucket_lock);
			/*
			 * Invalidate all shadow EPTPs of L1 VM
			 * TODO: Invalidating all L2 vCPU associated EPTPs is enough. How?
			 */
			for (i = 0L; i < CONFIG_MAX_GUEST_EPT_NUM; i++) {
				if (vept_desc_bucket[i].guest_eptp != 0UL) {
					desc = &vept_desc_bucket[i];
					free_sept_table((void *)(desc->shadow_eptp & PAGE_MASK));
					invept((void *)(desc->shadow_eptp & PAGE_MASK));
				}
			}
			spinlock_release(&vept_desc_bucket_lock);
			nested_vmx_result(VMsucceed, 0);
		} else {
			nested_vmx_result(VMfailValid, VMXERR_INVEPT_INVVPID_INVALID_OPERAND);
		}
	}

	return 0;
}

void init_vept(void)
{
	init_vept_pool();
	sept_page_pool.start_page = sept_pages;
	sept_page_pool.bitmap_size = calc_sept_page_num() / 64U;
	sept_page_pool.bitmap = sept_page_bitmap;
        sept_page_pool.dummy_page = NULL;
	spinlock_init(&sept_page_pool.lock);
	memset((void *)sept_page_pool.bitmap, 0, sept_page_pool.bitmap_size * sizeof(uint64_t));
	sept_page_pool.last_hint_id = 0UL;

	spinlock_init(&vept_desc_bucket_lock);
}
