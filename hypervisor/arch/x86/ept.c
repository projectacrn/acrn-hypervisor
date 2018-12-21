/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include "guest/instr_emul.h"

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
	uint64_t *pgentry, pg_size = 0UL;
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
uint64_t vm0_hpa2gpa(uint64_t hpa)
{
	return hpa;
}

int32_t ept_violation_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t status = -EINVAL, ret;
	uint64_t exit_qual;
	uint64_t gpa;
	struct io_request *io_req = &vcpu->req;
	struct mmio_request *mmio_req = &io_req->reqs.mmio;

	/* Handle page fault from guest */
	exit_qual = vcpu->arch.exit_qualification;

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
		/*
		 * For MMIO write, ask DM to run MMIO emulation after
		 * instruction emulation. For MMIO read, ask DM to run MMIO
		 * emulation at first.
		 */

		/* Determine value being written. */
		if (mmio_req->direction == REQUEST_WRITE) {
			status = emulate_instruction(vcpu);
			if (status != 0) {
				ret = -EFAULT;
			}
		}

		if (ret > 0) {
			status = emulate_io(vcpu, io_req);
			if (status == 0) {
				emulate_mmio_post(vcpu, io_req);
			} else {
				if (status == IOREQ_PENDING) {
					status = 0;
				}
			}
		}
	} else {
		if (ret == -EFAULT) {
			pr_info("page fault happen during decode_instruction");
			status = 0;
		}
	}

	if (ret <= 0) {
		pr_acrnlog("Guest Linear Address: 0x%016llx", exec_vmread(VMX_GUEST_LINEAR_ADDR));
		pr_acrnlog("Guest Physical Address address: 0x%016llx", gpa);
	}
	return status;
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
	if (((prot & EPT_MT_MASK) != EPT_UNCACHED) && vm->snoopy_mem) {
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

	if (((local_prot & EPT_MT_MASK) != EPT_UNCACHED) && vm->snoopy_mem) {
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
