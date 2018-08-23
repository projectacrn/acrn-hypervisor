/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include "guest/instr_emul.h"

#define ACRN_DBG_EPT	6U

static uint64_t find_next_table(uint32_t table_offset, const void *table_base)
{
	uint64_t table_entry;
	uint64_t table_present;
	uint64_t sub_table_addr = 0UL;

	/* Read the table entry */
	table_entry = mem_read64(table_base
			+ (table_offset * IA32E_COMM_ENTRY_SIZE));

	/* If bit 7 is set, entry is not a subtable. */
	if (((table_entry & IA32E_PDPTE_PS_BIT) != 0U)
	    || ((table_entry & IA32E_PDE_PS_BIT) != 0U)) {
		return sub_table_addr;
	}

	/* Set table present bits to any of the read/write/execute bits */
	table_present = EPT_RWX;

	/* Determine if a valid entry exists */
	if ((table_entry & table_present) == 0UL) {
		/* No entry present */
		return sub_table_addr;
	}

	/* Get address of the sub-table */
	sub_table_addr = table_entry & IA32E_REF_MASK;

	/* Return the next table in the walk */
	return sub_table_addr;
}

/**
 * @pre pml4_addr != NULL
 */
void free_ept_mem(void *pml4_addr)
{
	void *pdpt_addr;
	void *pde_addr;
	void *pte_addr;
	uint32_t pml4_index;
	uint32_t pdpt_index;
	uint32_t pde_idx;

	for (pml4_index = 0U; pml4_index < IA32E_NUM_ENTRIES; pml4_index++) {
		/* Walk from the PML4 table to the PDPT table */
		pdpt_addr = HPA2HVA(find_next_table(pml4_index, pml4_addr));
		if (pdpt_addr == NULL) {
			continue;
		}

		for (pdpt_index = 0U; pdpt_index < IA32E_NUM_ENTRIES;
				pdpt_index++) {
			/* Walk from the PDPT table to the PD table */
			pde_addr = HPA2HVA(find_next_table(pdpt_index,
						pdpt_addr));

			if (pde_addr == NULL) {
				continue;
			}

			for (pde_idx = 0U; pde_idx < IA32E_NUM_ENTRIES;
					pde_idx++) {
				/* Walk from the PD table to the page table */
				pte_addr = HPA2HVA(find_next_table(pde_idx,
						pde_addr));

				/* Free page table entry table */
				if (pte_addr != NULL) {
					free_paging_struct(pte_addr);
				}
			}
			/* Free page directory entry table */
			if (pde_addr != NULL) {
				free_paging_struct(pde_addr);
			}
		}
		free_paging_struct(pdpt_addr);
	}
	free_paging_struct(pml4_addr);
}

void destroy_ept(struct vm *vm)
{
	if (vm->arch_vm.nworld_eptp != NULL)
		free_ept_mem(vm->arch_vm.nworld_eptp);
	if (vm->arch_vm.m2p != NULL)
		free_ept_mem(vm->arch_vm.m2p);
}

uint64_t local_gpa2hpa(const struct vm *vm, uint64_t gpa, uint32_t *size)
{
	uint64_t hpa = 0UL;
	uint64_t *pgentry, pg_size = 0UL;
	void *eptp;
	struct vcpu *vcpu = vcpu_from_pid(vm, get_cpu_id());

	if (vcpu && (vcpu->arch_vcpu.cur_context == SECURE_WORLD)) {
		eptp = vm->arch_vm.sworld_eptp;
	} else {
		eptp = vm->arch_vm.nworld_eptp;
	}

	pgentry = lookup_address((uint64_t *)eptp, gpa, &pg_size, PTT_EPT);
	if (pgentry != NULL) {
		hpa = ((*pgentry & (~(pg_size - 1UL)))
				| (gpa & (pg_size - 1UL)));
		pr_dbg("GPA2HPA: 0x%llx->0x%llx", gpa, hpa);
	} else {
		pr_err("VM %d GPA2HPA: failed for gpa 0x%llx",
				vm->vm_id, gpa);
	}

	if (size != NULL) {
		*size = (uint32_t)pg_size;
	}

	return hpa;
}

/* using return value 0 as failure, make sure guest will not use hpa 0 */
uint64_t gpa2hpa(const struct vm *vm, uint64_t gpa)
{
	return local_gpa2hpa(vm, gpa, NULL);
}

uint64_t hpa2gpa(const struct vm *vm, uint64_t hpa)
{
	uint64_t *pgentry, pg_size = 0UL;

	pgentry = lookup_address((uint64_t *)vm->arch_vm.m2p,
			hpa, &pg_size, PTT_EPT);
	if (pgentry == NULL) {
		pr_err("VM %d hpa2gpa: failed for hpa 0x%llx",
				vm->vm_id, hpa);
		ASSERT(false, "hpa2gpa not found");
	}
	return ((*pgentry & (~(pg_size - 1UL)))
			| (hpa & (pg_size - 1UL)));
}

int ept_violation_vmexit_handler(struct vcpu *vcpu)
{
	int status = -EINVAL, ret;
	uint64_t exit_qual;
	uint64_t gpa;
	struct io_request *io_req = &vcpu->req;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;

	/* Handle page fault from guest */
	exit_qual = vcpu->arch_vcpu.exit_qualification;

	io_req->type = REQ_MMIO;

	/* Specify if read or write operation */
	if ((exit_qual & 0x2UL) != 0UL) {
		/* Write operation */
		mmio_req->direction = REQUEST_WRITE;
		mmio_req->value = 0UL;

		/* XXX: write access while EPT perm RX -> WP */
		if ((exit_qual & 0x38UL) == 0x28UL) {
			io_req->type = REQ_WP;
		}
	} else {
		/* Read operation */
		mmio_req->direction = REQUEST_READ;

		/* TODO: Need to determine how sign extension is determined for
		 * reads
		 */
	}

	/* Get the guest physical address */
	gpa = exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL);

	TRACE_2L(TRACE_VMEXIT_EPT_VIOLATION, exit_qual, gpa);

	/* Adjust IPA appropriately and OR page offset to get full IPA of abort
	 */
	mmio_req->address = gpa;

	ret = decode_instruction(vcpu);
	if (ret > 0) {
		mmio_req->size = (uint64_t)ret;
	} else if (ret == -EFAULT) {
		pr_info("page fault happen during decode_instruction");
		status = 0;
		goto out;
	} else {
		goto out;
	}


	/*
	 * For MMIO write, ask DM to run MMIO emulation after
	 * instruction emulation. For MMIO read, ask DM to run MMIO
	 * emulation at first.
	 */

	/* Determine value being written. */
	if (mmio_req->direction == REQUEST_WRITE) {
		status = emulate_instruction(vcpu);
		if (status != 0) {
			goto out;
		}
	}

	status = emulate_io(vcpu, io_req);

	if (status == 0) {
		emulate_mmio_post(vcpu, io_req);
	} else if (status == IOREQ_PENDING) {
		status = 0;
	}

	return status;

out:
	pr_acrnlog("Guest Linear Address: 0x%016llx",
			exec_vmread(VMX_GUEST_LINEAR_ADDR));

	pr_acrnlog("Guest Physical Address address: 0x%016llx",
			gpa);

	return status;
}

int ept_misconfig_vmexit_handler(__unused struct vcpu *vcpu)
{
	int status;

	status = -EINVAL;

	/* TODO - EPT Violation handler */
	pr_fatal("%s, Guest linear address: 0x%016llx ",
			__func__, exec_vmread(VMX_GUEST_LINEAR_ADDR));

	pr_fatal("%s, Guest physical address: 0x%016llx ",
			__func__, exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL));

	ASSERT(status == 0, "EPT Misconfiguration is not handled.\n");

	TRACE_2L(TRACE_VMEXIT_EPT_MISCONFIGURATION, 0UL, 0UL);

	return status;
}

int ept_mr_add(const struct vm *vm, uint64_t *pml4_page,
	uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig)
{
	uint16_t i;
	struct vcpu *vcpu;
	int ret;
	uint64_t prot = prot_orig;

	dev_dbg(ACRN_DBG_EPT, "%s, vm[%d] hpa: 0x%016llx gpa: 0x%016llx ",
			__func__, vm->vm_id, hpa, gpa);
	dev_dbg(ACRN_DBG_EPT, "size: 0x%016llx prot: 0x%016x\n", size, prot);

	/* EPT & VT-d share the same page tables, set SNP bit
	 * to force snooping of PCIe devices if the page
	 * is cachable
	 */
	if ((prot & EPT_MT_MASK) != EPT_UNCACHED) {
		prot |= EPT_SNOOP_CTRL;
	}

	ret = mmu_add(pml4_page, hpa, gpa, size, prot, PTT_EPT);
	/* No need to create inverted page tables for trusty memory */
	if (ret == 0 && ((void *)pml4_page == vm->arch_vm.nworld_eptp)) {
		ret = mmu_add((uint64_t *)vm->arch_vm.m2p,
			gpa, hpa, size, prot, PTT_EPT);
	}

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	return ret;
}

int ept_mr_modify(const struct vm *vm, uint64_t *pml4_page,
		uint64_t gpa, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr)
{
	struct vcpu *vcpu;
	uint16_t i;
	int ret;

	ret = mmu_modify_or_del(pml4_page, gpa, size,
			prot_set, prot_clr, PTT_EPT, MR_MODIFY);

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	return ret;
}

int ept_mr_del(const struct vm *vm, uint64_t *pml4_page,
		uint64_t gpa, uint64_t size)
{
	struct vcpu *vcpu;
	uint16_t i;
	int ret;
	uint64_t hpa = gpa2hpa(vm, gpa);

	dev_dbg(ACRN_DBG_EPT, "%s,vm[%d] gpa 0x%llx size 0x%llx\n",
			__func__, vm->vm_id, gpa, size);

	ret = mmu_modify_or_del(pml4_page, gpa, size,
			0UL, 0UL, PTT_EPT, MR_DEL);
	if ((ret == 0) && (hpa != 0UL)) {
		ret = mmu_modify_or_del((uint64_t *)vm->arch_vm.m2p,
				hpa, size, 0UL, 0UL, PTT_EPT, MR_DEL);
	}

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	return ret;
}
