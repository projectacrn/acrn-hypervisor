/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
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
#define CONFIG_MAX_GUEST_EPT_NUM	4
static struct nept_desc nept_desc_bucket[CONFIG_MAX_GUEST_EPT_NUM];
static spinlock_t nept_desc_bucket_lock;

/*
 * For simplicity, total platform RAM size is considered to calculate the
 * memory needed for shadow page tables. This is not an accurate upper bound.
 * This can satisfy typical use-cases where there is not a lot ofovercommitment
 * and sharing of memory between L2 VMs.
 *
 * Page table entry need 8 bytes to represent every 4K page frame.
 * Total number of bytes = (CONFIG_PLATFORM_RAM_SIZE/4096) * 8
 * Number of pages needed = Total number of bytes needed/4096
 */
#define TOTAL_SEPT_4K_PAGES_SIZE	((CONFIG_PLATFORM_RAM_SIZE * 8UL) / 4096UL)
#define TOTAL_SEPT_4K_PAGES_NUM		(TOTAL_SEPT_4K_PAGES_SIZE / PAGE_SIZE)

static struct page_pool sept_page_pool;
static struct page *sept_pages;
static uint64_t sept_page_bitmap[TOTAL_SEPT_4K_PAGES_NUM / 64U];

/*
 * @brief Reserve space for SEPT 4K pages from platform E820 table
 * 	  At moment, we only support nested VMX for SOS VM.
 */
void reserve_buffer_for_sept_pages(void)
{
	uint64_t page_base;

	page_base = e820_alloc_memory(TOTAL_SEPT_4K_PAGES_SIZE, ~0UL);
	set_paging_supervisor(page_base, TOTAL_SEPT_4K_PAGES_SIZE);
	sept_pages = (struct page *)page_base;
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
 * @brief Convert a guest EPTP to the associated nept_desc.
 * @return struct nept_desc * if existed.
 * @return NULL if non-existed.
 */
static struct nept_desc *find_nept_desc(uint64_t guest_eptp)
{
	uint32_t i;
	struct nept_desc *desc = NULL;

	if (guest_eptp) {
		spinlock_obtain(&nept_desc_bucket_lock);
		for (i = 0L; i < CONFIG_MAX_GUEST_EPT_NUM; i++) {
			/* Find an existed nept_desc of the guest EPTP */
			if (nept_desc_bucket[i].guest_eptp == guest_eptp) {
				desc = &nept_desc_bucket[i];
				break;
			}
		}
		spinlock_release(&nept_desc_bucket_lock);
	}

	return desc;
}

/*
 * @brief Convert a guest EPTP to a shadow EPTP.
 * @return 0 if non-existed.
 */
uint64_t get_shadow_eptp(uint64_t guest_eptp)
{
	struct nept_desc *desc = NULL;

	desc = find_nept_desc(guest_eptp);
	return (desc != NULL) ? hva2hpa((void *)desc->shadow_eptp) : 0UL;
}

/*
 * @brief Get a nept_desc to cache a guest EPTP
 *
 * If there is already an existed nept_desc associated with given guest_eptp,
 * increase its ref_count and return it. If there is not existed nept_desc
 * for guest_eptp, create one and initialize it.
 *
 * @return a nept_desc which associate the guest EPTP with a shadow EPTP
 */
struct nept_desc *get_nept_desc(uint64_t guest_eptp)
{
	uint32_t i;
	struct nept_desc *desc = NULL;

	if (guest_eptp != 0UL) {
		spinlock_obtain(&nept_desc_bucket_lock);
		for (i = 0L; i < CONFIG_MAX_GUEST_EPT_NUM; i++) {
			/* Find an existed nept_desc of the guest EPTP address bits */
			if (nept_desc_bucket[i].guest_eptp == guest_eptp) {
				desc = &nept_desc_bucket[i];
				desc->ref_count++;
				break;
			}
			/* Get the first empty nept_desc for the guest EPTP */
			if (!desc && (nept_desc_bucket[i].ref_count == 0UL)) {
				desc = &nept_desc_bucket[i];
			}
		}
		ASSERT(desc != NULL, "Get nept_desc failed!");

		/* A new nept_desc, initialize it */
		if (desc->shadow_eptp == 0UL) {
			desc->shadow_eptp = (uint64_t)alloc_page(&sept_page_pool) | (guest_eptp & ~PAGE_MASK);
			desc->guest_eptp = guest_eptp;
			desc->ref_count = 1UL;

			dev_dbg(VETP_LOG_LEVEL, "[%s], nept_desc[%llx] ref[%d] shadow_eptp[%llx] guest_eptp[%llx]",
					__func__, desc, desc->ref_count, desc->shadow_eptp, desc->guest_eptp);
		}

		spinlock_release(&nept_desc_bucket_lock);
	}

	return desc;
}

/*
 * @brief Put a nept_desc who associate with a guest_eptp
 *
 * If ref_count of the nept_desc, then release all resources used by it.
 */
void put_nept_desc(uint64_t guest_eptp)
{
	struct nept_desc *desc = NULL;

	if (guest_eptp != 0UL) {
		desc = find_nept_desc(guest_eptp);
		spinlock_obtain(&nept_desc_bucket_lock);
		if (desc) {
			desc->ref_count--;
			if (desc->ref_count == 0UL) {
				dev_dbg(VETP_LOG_LEVEL, "[%s], nept_desc[%llx] ref[%d] shadow_eptp[%llx] guest_eptp[%llx]",
						__func__, desc, desc->ref_count, desc->shadow_eptp, desc->guest_eptp);
				free_page(&sept_page_pool, (struct page *)(desc->shadow_eptp & PAGE_MASK));
				/* Flush the hardware TLB */
				invept((void *)(desc->shadow_eptp & PAGE_MASK));
				desc->shadow_eptp = 0UL;
				desc->guest_eptp = 0UL;
			}
		}
		spinlock_release(&nept_desc_bucket_lock);
	}
}

/**
 * @brief Shadow a guest EPT entry
 * @pre vcpu != NULL
 */
static uint64_t generate_shadow_ept_entry(struct acrn_vcpu *vcpu, uint64_t guest_ept_entry,
				    enum _page_table_level guest_level)
{
	uint64_t shadow_ept_entry;

	/*
	 * Clone a shadow EPT entry w/o physical address bits from guest EPT entry
	 * TODO:
	 *   Before the cloning, host EPT mapping audit is a necessary.
	 */
	shadow_ept_entry = guest_ept_entry & ~EPT_ENTRY_PFN_MASK;
	if (is_leaf_ept_entry(guest_ept_entry, guest_level)) {
		/*
		 * Use guest EPT entry HPA in shadow EPT entry
		 */
		shadow_ept_entry |= gpa2hpa(vcpu->vm, (guest_ept_entry & EPT_ENTRY_PFN_MASK));
	} else {
		/* Use a HPA of a new page in shadow EPT entry */
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
	if (!pcpu_has_vmx_ept_cap(VMX_EPT_EXECUTE_ONLY)) {
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
	uint64_t guest_eptp = vcpu->arch.nested.vmcs12.ept_pointer;
	struct nept_desc *desc = find_nept_desc(guest_eptp);
	uint64_t l2_ept_violation_gpa = exec_vmread(VMX_GUEST_PHYSICAL_ADDR_FULL);
	enum _page_table_level pt_level;
	uint64_t guest_ept_entry, shadow_ept_entry;
	uint64_t *p_guest_ept_page, *p_shadow_ept_page;
	uint16_t offset;
	bool is_l1_vmexit = true;

	ASSERT(desc != NULL, "Invalid shadow EPTP!");

	spinlock_obtain(&nept_desc_bucket_lock);
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
		}

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
	spinlock_release(&nept_desc_bucket_lock);

	return is_l1_vmexit;
}

/**
 * @pre vcpu != NULL
 */
int32_t invept_vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct invept_desc operand_gla_ept;
	uint64_t type;

	if (check_vmx_permission(vcpu)) {
		type = get_invvpid_ept_operands(vcpu, (void *)&operand_gla_ept, sizeof(operand_gla_ept));

		if (type > INVEPT_TYPE_ALL_CONTEXTS) {
			nested_vmx_result(VMfailValid, VMXERR_INVEPT_INVVPID_INVALID_OPERAND);
		} else {
			operand_gla_ept.eptp = gpa2hpa(vcpu->vm, operand_gla_ept.eptp);
			asm_invept(type, operand_gla_ept);
			nested_vmx_result(VMsucceed, 0);
		}
	}

	return 0;
}

void init_vept(void)
{
	sept_page_pool.start_page = sept_pages;
	sept_page_pool.bitmap_size = TOTAL_SEPT_4K_PAGES_NUM / 64U;
	sept_page_pool.bitmap = sept_page_bitmap;
	sept_page_pool.dummy_page = NULL;
	spinlock_init(&sept_page_pool.lock);
	memset((void *)sept_page_pool.bitmap, 0, sept_page_pool.bitmap_size * sizeof(uint64_t));
	sept_page_pool.last_hint_id = 0UL;

	spinlock_init(&nept_desc_bucket_lock);
}
