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
#include <asm/guest/nested.h>

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
}
