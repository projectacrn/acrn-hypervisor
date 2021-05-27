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
 * @pre vcpu != NULL
 */
bool handle_l2_ept_violation(__unused struct acrn_vcpu *vcpu)
{
	/* TODO: Construct the shadow page table for EPT violation address */
	return true;
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
