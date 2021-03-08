/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
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
#include <rtct.h>

#define DBG_LEVEL_EPT	6U

/*
 * To enable the identical map and support of legacy devices/ACPI method in SOS,
 * ACRN presents the entire host 0-4GB memory region to SOS, except the memory
 * regions explicitly assigned to pre-launched VMs or HV (DRAM and MMIO). However,
 * virtual e820 only contains the known DRAM regions. For this reason,
 * we can't know if the GPA range is guest valid or not, by checking with
 * its ve820 tables only.
 *
 * instead, we Check if the GPA range is guest valid by whether the GPA range is mapped
 * in EPT pagetable or not
 */
bool ept_is_valid_mr(struct acrn_vm *vm, uint64_t mr_base_gpa, uint64_t mr_size)
{
	bool present = true;
	uint32_t sz;
	uint64_t end = mr_base_gpa + mr_size, address = mr_base_gpa;

	while (address < end) {
		if (local_gpa2hpa(vm, address, &sz) == INVALID_HPA) {
			present = false;
			break;
		}
		address += sz;
	}

	return present;
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
	pgentry = lookup_address((uint64_t *)eptp, gpa, &pg_size, &vm->arch_vm.ept_pgtable);
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

	mmu_add(pml4_page, hpa, gpa, size, prot, &vm->arch_vm.ept_pgtable);

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

	mmu_modify_or_del(pml4_page, gpa, size, local_prot, prot_clr, &(vm->arch_vm.ept_pgtable), MR_MODIFY);

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

	mmu_modify_or_del(pml4_page, gpa, size, 0UL, 0UL, &vm->arch_vm.ept_pgtable, MR_DEL);

	spinlock_release(&vm->ept_lock);

	ept_flush_guest(vm);
}

/**
 * @pre pge != NULL && size > 0.
 */
void ept_flush_leaf_page(uint64_t *pge, uint64_t size)
{
	uint64_t base_hpa, end_hpa;
	uint64_t sw_sram_bottom, sw_sram_top;

	if ((*pge & EPT_MT_MASK) != EPT_UNCACHED) {
		base_hpa = (*pge & (~(size - 1UL)));
		end_hpa = base_hpa + size;

		 sw_sram_bottom = get_software_sram_base();
		 sw_sram_top = sw_sram_bottom + get_software_sram_size();
		/* When Software SRAM is not initialized, both sw_sram_bottom and sw_sram_top is 0,
		 * so the first if below will have no use.
		 */
		if (base_hpa < sw_sram_bottom) {
			/*
			 * For end_hpa < sw_sram_bottom, flush [base_hpa, end_hpa);
			 * For end_hpa >= sw_sram_bottom && end_hpa < sw_sram_top, flush [base_hpa, sw_sram_bottom);
			 * For end_hpa > sw_sram_top, flush [base_hpa, sw_sram_bottom) first,
			 *                            flush [sw_sram_top, end_hpa) in the next if condition
			 */
			stac();
			flush_address_space(hpa2hva(base_hpa), min(end_hpa, sw_sram_bottom) - base_hpa);
			clac();
		}

		if (end_hpa > sw_sram_top) {
			/*
			 * For base_hpa > sw_sram_top, flush [base_hpa, end_hpa);
			 * For base_hpa >= sw_sram_bottom && base_hpa < sw_sram_top, flush [sw_sram_top, end_hpa);
			 * For base_hpa < sw_sram_bottom, flush [sw_sram_top, end_hpa) here,
			 *                            flush [base_hpa, sw_sram_bottom) in the below if condition
			 */
			stac();
			flush_address_space(hpa2hva(max(base_hpa, sw_sram_top)), end_hpa - max(base_hpa, sw_sram_top));
			clac();
		}
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
	const struct pgtable *table = &vm->arch_vm.ept_pgtable;
	uint64_t *pml4e, *pdpte, *pde, *pte;
	uint64_t i, j, k, m;

	for (i = 0UL; i < PTRS_PER_PML4E; i++) {
		pml4e = pml4e_offset((uint64_t *)get_ept_entry(vm), i << PML4E_SHIFT);
		if (table->pgentry_present(*pml4e) == 0UL) {
			continue;
		}
		for (j = 0UL; j < PTRS_PER_PDPTE; j++) {
			pdpte = pdpte_offset(pml4e, j << PDPTE_SHIFT);
			if (table->pgentry_present(*pdpte) == 0UL) {
				continue;
			}
			if (pdpte_large(*pdpte) != 0UL) {
				cb(pdpte, PDPTE_SIZE);
				continue;
			}
			for (k = 0UL; k < PTRS_PER_PDE; k++) {
				pde = pde_offset(pdpte, k << PDE_SHIFT);
				if (table->pgentry_present(*pde) == 0UL) {
					continue;
				}
				if (pde_large(*pde) != 0UL) {
					cb(pde, PDE_SIZE);
					continue;
				}
				for (m = 0UL; m < PTRS_PER_PTE; m++) {
					pte = pte_offset(pde, m << PTE_SHIFT);
					if (table->pgentry_present(*pte) != 0UL) {
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

struct page *alloc_ept_page(struct acrn_vm *vm)
{
	return alloc_page(vm->arch_vm.ept_pgtable.pool);
}
