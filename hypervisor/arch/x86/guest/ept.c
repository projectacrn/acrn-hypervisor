/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include <instr_emul.h>
#include <vtd.h>

#define ACRN_DBG_EPT	6U

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

/* using return value INVALID_HPA as error code */
uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size)
{
	uint64_t hpa = INVALID_HPA;
	const uint64_t *pgentry;
	uint64_t pg_size = 0UL;
	void *eptp;
	struct acrn_vcpu *vcpu = vcpu_from_pid(vm, get_cpu_id());

	if ((vcpu != NULL) && (vcpu->arch.cur_context == SECURE_WORLD)) {
		eptp = vm->arch_vm.sworld_eptp;
	} else {
		eptp = vm->arch_vm.nworld_eptp;
	}

	pgentry = lookup_address((uint64_t *)eptp, gpa, &pg_size, &vm->arch_vm.ept_mem_ops);
	if (pgentry != NULL) {
		hpa = ((*pgentry & (~(pg_size - 1UL)))
				| (gpa & (pg_size - 1UL)));
		pr_dbg("GPA2HPA: 0x%llx->0x%llx", gpa, hpa);
	} else {
		pr_err("VM %d GPA2HPA: failed for gpa 0x%llx",
				vm->vm_id, gpa);
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
	pr_fatal("%s, Guest linear address: 0x%016llx ",
			__func__, exec_vmread(VMX_GUEST_LINEAR_ADDR));

	pr_fatal("%s, Guest physical address: 0x%016llx ",
			__func__, exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL));

	ASSERT(status == 0, "EPT Misconfiguration is not handled.\n");

	TRACE_2L(TRACE_VMEXIT_EPT_MISCONFIGURATION, 0UL, 0UL);

	return status;
}

void ept_mr_add(struct acrn_vm *vm, uint64_t *pml4_page,
	uint64_t hpa, uint64_t gpa, uint64_t size, uint64_t prot_orig)
{
	uint16_t i;
	struct acrn_vcpu *vcpu;
	uint64_t prot = prot_orig;

	dev_dbg(ACRN_DBG_EPT, "%s, vm[%d] hpa: 0x%016llx gpa: 0x%016llx size: 0x%016llx prot: 0x%016x\n",
			__func__, vm->vm_id, hpa, gpa, size, prot);

	/* EPT & VT-d share the same page tables, set SNP bit
	 * to force snooping of PCIe devices if the page
	 * is cachable
	 */
	if (((prot & EPT_MT_MASK) != EPT_UNCACHED) && iommu_snoop_supported(vm->iommu)) {
		prot |= EPT_SNOOP_CTRL;
	}

	mmu_add(pml4_page, hpa, gpa, size, prot, &vm->arch_vm.ept_mem_ops);

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

void ept_mr_modify(struct acrn_vm *vm, uint64_t *pml4_page,
		uint64_t gpa, uint64_t size,
		uint64_t prot_set, uint64_t prot_clr)
{
	struct acrn_vcpu *vcpu;
	uint16_t i;
	uint64_t local_prot = prot_set;

	dev_dbg(ACRN_DBG_EPT, "%s,vm[%d] gpa 0x%llx size 0x%llx\n", __func__, vm->vm_id, gpa, size);

	if (((local_prot & EPT_MT_MASK) != EPT_UNCACHED) && iommu_snoop_supported(vm->iommu)) {
		local_prot |= EPT_SNOOP_CTRL;
	}

	mmu_modify_or_del(pml4_page, gpa, size, local_prot, prot_clr, &(vm->arch_vm.ept_mem_ops), MR_MODIFY);

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}
/**
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void ept_mr_del(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa, uint64_t size)
{
	struct acrn_vcpu *vcpu;
	uint16_t i;

	dev_dbg(ACRN_DBG_EPT, "%s,vm[%d] gpa 0x%llx size 0x%llx\n", __func__, vm->vm_id, gpa, size);

	mmu_modify_or_del(pml4_page, gpa, size, 0UL, 0UL, &vm->arch_vm.ept_mem_ops, MR_DEL);

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}
