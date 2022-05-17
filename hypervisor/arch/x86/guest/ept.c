/*
 * Copyright (C) 2018-2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <vm.h>
#include <irq.h>
#include <pgtable.h>
#include <mmu.h>
#include <ept.h>
#include <vmx.h>
#include <vtd.h>
#include <logmsg.h>
#include <trace.h>
#include <ptct.h>

#define DBG_LEVEL_EPT	6U

bool ept_is_mr_valid(const struct acrn_vm *vm, uint64_t base, uint64_t size)
{
	bool valid = true;
	uint64_t end = base + size;
	uint64_t top_address_space = vm->arch_vm.ept_mem_ops.info->ept.top_address_space;
	if ((end <= base) || (end > top_address_space)) {
		valid = false;
	}

	return valid;
}

void destroy_ept(struct acrn_vm *vm)
{
	/* Destroy secure world */
	if (vm->sworld_control.flag.active != 0UL) {
		destroy_secure_world(vm, true);
	}

	if (vm->arch_vm.nworld_eptp != NULL) {
		(void)memset(vm->arch_vm.nworld_eptp, 0U, PAGE_SIZE);
	}
}

/**
 * @pre: vm != NULL.
 */
uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size)
{
	/* using return value INVALID_HPA as error code */
	uint64_t hpa = INVALID_HPA;
	const uint64_t *pgentry;
	uint64_t pg_size = 0UL;
	void *eptp;

	eptp = get_ept_entry(vm);
	pgentry = lookup_address((uint64_t *)eptp, gpa, &pg_size, &vm->arch_vm.ept_mem_ops);
	if (pgentry != NULL) {
		hpa = (((*pgentry & (~EPT_PFN_HIGH_MASK)) & (~(pg_size - 1UL)))
				| (gpa & (pg_size - 1UL)));
	}

	/**
	 * If specified parameter size is not NULL and
	 * the HPA of parameter gpa is found, pg_size shall
	 * be returned through parameter size.
	 */
	if ((size != NULL) && (hpa != INVALID_HPA)) {
		*size = (uint32_t)pg_size;
	}

	return hpa;
}

/* using return value INVALID_HPA as error code */
uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa)
{
	return local_gpa2hpa(vm, gpa, NULL);
}

/**
 * @pre: the gpa and hpa are identical mapping in SOS.
 */
uint64_t sos_vm_hpa2gpa(uint64_t hpa)
{
	return hpa;
}

int32_t ept_misconfig_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	int32_t status;

	status = -EINVAL;

	/* TODO - EPT Violation handler */
	pr_fatal("%s, Guest linear address: 0x%016lx ",
			__func__, exec_vmread(VMX_GUEST_LINEAR_ADDR));

	pr_fatal("%s, Guest physical address: 0x%016lx ",
			__func__, exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL));

	ASSERT(status == 0, "EPT Misconfiguration is not handled.\n");

	TRACE_2L(TRACE_VMEXIT_EPT_MISCONFIGURATION, 0UL, 0UL);

	return status;
}

static inline void ept_flush_guest(struct acrn_vm *vm)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	/* Here doesn't do the real flush, just makes the request which will be handled before vcpu vmenter */
	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

void ept_add_mr(struct acrn_vm *vm, uint64_t *pml4_page,
	uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig)
{
	uint64_t prot = prot_orig;

	dev_dbg(DBG_LEVEL_EPT, "%s, vm[%d] hpa: 0x%016lx gpa: 0x%016lx size: 0x%016lx prot: 0x%016x\n",
			__func__, vm->vm_id, hpa, gpa, size, prot);

	spinlock_obtain(&vm->ept_lock);

	mmu_add(pml4_page, hpa, gpa, size, prot, &vm->arch_vm.ept_mem_ops);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}

void ept_modify_mr(struct acrn_vm *vm, uint64_t *pml4_page,
		uint64_t gpa, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr)
{
	uint64_t local_prot = prot_set;

	dev_dbg(DBG_LEVEL_EPT, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	spinlock_obtain(&vm->ept_lock);

	mmu_modify_or_del(pml4_page, gpa, size, local_prot, prot_clr, &(vm->arch_vm.ept_mem_ops), MR_MODIFY);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}
/**
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void ept_del_mr(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa, uint64_t size)
{
	dev_dbg(DBG_LEVEL_EPT, "%s,vm[%d] gpa 0x%lx size 0x%lx\n", __func__, vm->vm_id, gpa, size);

	spinlock_obtain(&vm->ept_lock);

	mmu_modify_or_del(pml4_page, gpa, size, 0UL, 0UL, &vm->arch_vm.ept_mem_ops, MR_DEL);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}

/**
 * @pre pge != NULL && size > 0.
 */
void ept_flush_leaf_page(uint64_t *pge, uint64_t size)
{
	uint64_t flush_base_hpa = INVALID_HPA, flush_end_hpa;
	void *hva = NULL;
	uint64_t flush_size = size;

	if ((*pge & EPT_MT_MASK) != EPT_UNCACHED) {
		flush_base_hpa = (*pge & (~(size - 1UL)));
		flush_end_hpa = flush_base_hpa + size;

		/* When pSRAM is not intialized, both psram_area_bottom and psram_area_top is 0,
		 * so the below if/else will have no use
		 */
		if (flush_base_hpa < psram_area_bottom) {
			/* Only flush [flush_base_hpa, psram_area_bottom) and [psram_area_top, flush_base_hpa),
			 * ignore [psram_area_bottom, psram_area_top)
			 */
			if (flush_end_hpa > psram_area_top) {
				/* Only flush [flush_base_hpa, psram_area_bottom) and [psram_area_top, flush_base_hpa),
				 * ignore [psram_area_bottom, psram_area_top)
				 */
				flush_size = psram_area_bottom - flush_base_hpa;
				hva = hpa2hva(flush_base_hpa);
				stac();
				flush_address_space(hva, flush_size);
				clac();

				flush_size = flush_end_hpa - psram_area_top;
				flush_base_hpa = psram_area_top;
			} else if (flush_end_hpa > psram_area_bottom) {
				/* Only flush [flush_base_hpa, psram_area_bottom) and
				 * ignore [psram_area_bottom, flush_end_hpa)
				 */
				flush_size = psram_area_bottom - flush_base_hpa;
			}
		} else if (flush_base_hpa < psram_area_top) {
			if (flush_end_hpa <= psram_area_top) {
				flush_size = 0UL;
			} else {
				/* Only flush [psram_area_top, flush_end_hpa) and ignore [flush_base_hpa, psram_area_top) */
				flush_base_hpa = psram_area_top;
				flush_size = flush_end_hpa - psram_area_top;
			}
		}

		hva = hpa2hva(flush_base_hpa);
		stac();
		flush_address_space(hva, flush_size);
		clac();
	}
}

/**
 * @pre: vm != NULL.
 */
void *get_ept_entry(struct acrn_vm *vm)
{
	void *eptp;
	struct acrn_vcpu *vcpu = vcpu_from_pid(vm, get_pcpu_id());

	if ((vcpu != NULL) && (vcpu->arch.cur_context == SECURE_WORLD)) {
		eptp = vm->arch_vm.sworld_eptp;
	} else {
		eptp = vm->arch_vm.nworld_eptp;
	}

	return eptp;
}

/**
 * @pre vm != NULL && cb != NULL.
 */
void walk_ept_table(struct acrn_vm *vm, pge_handler cb)
{
	const struct memory_ops *mem_ops = &vm->arch_vm.ept_mem_ops;
	uint64_t *pml4e, *pdpte, *pde, *pte;
	uint64_t i, j, k, m;

	for (i = 0UL; i < PTRS_PER_PML4E; i++) {
		pml4e = pml4e_offset((uint64_t *)get_ept_entry(vm), i << PML4E_SHIFT);
		if (mem_ops->pgentry_present(*pml4e) == 0UL) {
			continue;
		}
		for (j = 0UL; j < PTRS_PER_PDPTE; j++) {
			pdpte = pdpte_offset(pml4e, j << PDPTE_SHIFT);
			if (mem_ops->pgentry_present(*pdpte) == 0UL) {
				continue;
			}
			if (pdpte_large(*pdpte) != 0UL) {
				cb(pdpte, PDPTE_SIZE);
				continue;
			}
			for (k = 0UL; k < PTRS_PER_PDE; k++) {
				pde = pde_offset(pdpte, k << PDE_SHIFT);
				if (mem_ops->pgentry_present(*pde) == 0UL) {
					continue;
				}
				if (pde_large(*pde) != 0UL) {
					cb(pde, PDE_SIZE);
					continue;
				}
				for (m = 0UL; m < PTRS_PER_PTE; m++) {
					pte = pte_offset(pde, m << PTE_SHIFT);
					if (mem_ops->pgentry_present(*pte) != 0UL) {
						cb(pte, PTE_SIZE);
					}
				}
			}
			/*
			 * Walk through the whole page tables of one VM is a time-consuming
			 * operation. Preemption is not support by hypervisor scheduling
			 * currently, so the walk through page tables operation might occupy
			 * CPU for long time what starve other threads.
			 *
			 * Give chance to release CPU to make other threads happy.
			 */
			if (need_reschedule(get_pcpu_id())) {
				schedule();
			}
		}
	}
}
