/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>


#ifdef WORKAROUND_FOR_TRUSTY_4G_MEM

uint64_t gpa2hpa_for_trusty(struct vm *vm, uint64_t gpa)
{
	uint64_t hpa = 0UL;
	struct entry_params entry;
	struct map_params map_params;

	map_params.page_table_type = PTT_EPT;
	map_params.pml4_base = HPA2HVA(vm->arch_vm.sworld_eptp);
	map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);
	obtain_last_page_table_entry(&map_params, &entry, (void *)gpa, true);
	if (entry.entry_present == PT_PRESENT) {
		hpa = ((entry.entry_val & (~(entry.page_size - 1UL)))
				| (gpa & (entry.page_size - 1UL)));
		pr_dbg("GPA2HPA: 0x%llx->0x%llx", gpa, hpa);
	} else {
		pr_err("VM %d GPA2HPA: failed for gpa 0x%llx",
				vm->attr.boot_idx, gpa);
	}

	return hpa;
}

/**
 * @defgroup trusty_apis Trusty APIs
 *
 * This is a special group that includes all APIs
 * related to Trusty
 *
 * @{
 */

/**
 * @brief Create Secure World EPT hierarchy
 *
 * Create Secure World EPT hierarchy, construct new PML4/PDPT,
 * reuse PD/PT parse from vm->arch_vm->ept
 *
 * @param vm pointer to a VM with 2 Worlds
 * @param gpa_orig original gpa allocated from vSBL
 * @param size LK size (16M by default)
 * @param gpa_rebased gpa rebased to offset xxx (511G_OFFSET)
 *
 */
void create_secure_world_ept(struct vm *vm, uint64_t gpa_orig,
		uint64_t size, uint64_t gpa_rebased)
{
	uint64_t nworld_pml4e = 0UL;
	uint64_t sworld_pml4e = 0UL;
	struct entry_params entry;
	struct map_params map_params;
	uint64_t gpa_uos = gpa_orig;
	uint64_t gpa_sos;
	uint64_t adjust_size;
	uint64_t mod;
	uint64_t hpa = gpa2hpa(vm, gpa_uos);
	void *sub_table_addr = NULL, *pml4_base = NULL;
	int i;
	struct vm *vm0 = get_vm_from_vmid(0);
	struct vcpu *vcpu;

	if (vm0 == NULL) {
		pr_err("Parse vm0 context failed.");
		return;
	}

	if (!vm->sworld_control.sworld_enabled
			|| vm->arch_vm.sworld_eptp != 0UL) {
		pr_err("Sworld is not enabled or Sworld eptp is not NULL");
		return;
	}

	/* Backup secure world info, will be used when
	 * destroy secure world
	 */
	vm->sworld_control.sworld_memory.base_gpa = gpa_rebased;
	vm->sworld_control.sworld_memory.base_hpa = hpa;
	vm->sworld_control.sworld_memory.length = (uint64_t)size;
	vm->sworld_control.sworld_memory.gpa_sos = hpa2gpa(vm0, hpa);
	vm->sworld_control.sworld_memory.gpa_uos = gpa_orig;

	/* Copy PDPT entries from Normal world to Secure world
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 * The PML4/PDPT for Secure world are separated from
	 * Normal World.PD/PT are shared in both Secure world's EPT
	 * and Normal World's EPT
	 */
	pml4_base = alloc_paging_struct();
	vm->arch_vm.sworld_eptp = HVA2HPA(pml4_base);

	/* The trusty memory is remapped to guest physical address
	 * of gpa_rebased to gpa_rebased + size
	 */
	sub_table_addr = alloc_paging_struct();
	sworld_pml4e = HVA2HPA(sub_table_addr) | IA32E_EPT_R_BIT |
				IA32E_EPT_W_BIT |
				IA32E_EPT_X_BIT;
	mem_write64(pml4_base, sworld_pml4e);

	nworld_pml4e = mem_read64(HPA2HVA(vm->arch_vm.nworld_eptp));
	(void)memcpy_s(HPA2HVA(sworld_pml4e & IA32E_REF_MASK), CPU_PAGE_SIZE,
			HPA2HVA(nworld_pml4e & IA32E_REF_MASK), CPU_PAGE_SIZE);

	/* Unmap gpa_orig~gpa_orig+size from guest normal world ept mapping */
	map_params.page_table_type = PTT_EPT;

	while (size > 0) {
		map_params.pml4_base = HPA2HVA(vm->arch_vm.nworld_eptp);
		map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);
		obtain_last_page_table_entry(&map_params, &entry,
				(void *)gpa_uos, true);
		mod = (gpa_uos % entry.page_size);
		adjust_size = (mod) ? (entry.page_size - mod) : entry.page_size;
		if ((uint64_t)size < entry.page_size)
			adjust_size = size;
		hpa = gpa2hpa(vm, gpa_uos);

		/* Unmap from normal world */
		unmap_mem(&map_params, (void *)hpa,
			(void *)gpa_uos, adjust_size, 0U);

		/* Map to secure world */
		map_params.pml4_base = HPA2HVA(vm->arch_vm.sworld_eptp);
		map_mem(&map_params, (void *)hpa,
			(void *)gpa_rebased, adjust_size,
			(IA32E_EPT_R_BIT |
			 IA32E_EPT_W_BIT |
			 IA32E_EPT_X_BIT |
			 IA32E_EPT_WB));

		/* Unmap trusty memory space from sos ept mapping*/
		map_params.pml4_base = HPA2HVA(vm0->arch_vm.nworld_eptp);
		map_params.pml4_inverted = HPA2HVA(vm0->arch_vm.m2p);
		/* Get the gpa address in SOS */
		gpa_sos = hpa2gpa(vm0, hpa);

		unmap_mem(&map_params, (void *)hpa,
				(void *)gpa_sos, adjust_size, 0U);
		gpa_uos += adjust_size;
		size -= adjust_size;
		gpa_rebased += adjust_size;
	}

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	foreach_vcpu(i, vm0, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}

void  destroy_secure_world(struct vm *vm)
{

	struct map_params  map_params;
	struct entry_params entry;
	struct vm *vm0 = get_vm_from_vmid(0);
	uint64_t hpa;
	uint64_t mod;
	int i;
	struct vcpu *vcpu;
	uint64_t adjust_size;
	uint64_t gpa = vm->sworld_control.sworld_memory.base_gpa;
	int64_t size = (int64_t)vm->sworld_control.sworld_memory.length;

	if (vm0 == NULL) {
		pr_err("Parse vm0 context failed.");
		return;
	}

	map_params.page_table_type = PTT_EPT;
	while (size > 0) {
		/* clear trusty memory space */
		map_params.pml4_base = HPA2HVA(vm->arch_vm.sworld_eptp);
		map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);
		obtain_last_page_table_entry(&map_params, &entry,
				(void *)gpa, true);
		hpa = gpa2hpa_for_trusty(vm, gpa);
		mod = (hpa % entry.page_size);
		adjust_size = (mod) ? (entry.page_size - mod) : entry.page_size;
		if ((uint64_t)size < entry.page_size)
			adjust_size = size;

		(void)memset(HPA2HVA(hpa), 0, adjust_size);
		/* restore memory to SOS ept mapping */
		map_params.pml4_base = HPA2HVA(vm0->arch_vm.nworld_eptp);
		map_params.pml4_inverted = HPA2HVA(vm0->arch_vm.m2p);
		/* here gpa=hpa because sos 1:1 mapping
		 * this is a temp solution
		 */
		map_mem(&map_params, (void *)hpa,
				(void *)hpa,
				adjust_size,
				(IA32E_EPT_R_BIT |
				 IA32E_EPT_W_BIT |
				 IA32E_EPT_X_BIT |
				 IA32E_EPT_WB));
		size -= adjust_size;
		gpa += adjust_size;

	}

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	foreach_vcpu(i, vm0, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}
}
#endif
