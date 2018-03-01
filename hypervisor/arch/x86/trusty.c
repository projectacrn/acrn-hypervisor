/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>

void create_secure_world_ept(struct vm *vm, uint64_t gpa,
		uint64_t size, uint64_t rebased_gpa)
{
	int i = 0;
	uint64_t pml4e = 0;
	uint64_t entry = 0;
	struct map_params map_params;
	uint64_t hpa = gpa2hpa(vm, gpa);
	struct vm *vm0 = get_vm_from_vmid(0);

	/* Create secure world eptp */
	if (vm->sworld_control.sworld_enabled && !vm->arch_vm.sworld_eptp)
		vm->arch_vm.sworld_eptp = alloc_paging_struct();

	map_params.page_table_type = PT_EPT;
	map_params.pml4_inverted = vm->arch_vm.m2p;

	/* unmap gpa~gpa+size from guest ept mapping */
	map_params.pml4_base = vm->arch_vm.nworld_eptp;
	unmap_mem(&map_params, (void *)hpa, (void *)gpa, size, 0);

	/* Copy PDPT entries from Normal world to Secure world
	 * Secure world can access Normal World's memory,
	 * but Normal World can not access Secure World's memory.
	 * The PML4/PDPT for Secure world are separated from Normal World.
	 * PD/PT are shared in both Secure world's EPT and Normal World's EPT
	 */
	for (i = 0; i < IA32E_NUM_ENTRIES; i++) {
		pml4e = MEM_READ64(vm->arch_vm.nworld_eptp);
		entry = MEM_READ64((pml4e & IA32E_REF_MASK)
				+ (i * IA32E_COMM_ENTRY_SIZE));
		pml4e = MEM_READ64(vm->arch_vm.sworld_eptp);
		MEM_WRITE64((pml4e & IA32E_REF_MASK)
				+ (i * IA32E_COMM_ENTRY_SIZE),
				entry);
	}

	/* Map rebased_gpa~rebased_gpa+size
	 * to secure ept mapping
	 */
	map_params.pml4_base = vm->arch_vm.sworld_eptp;
	map_mem(&map_params, (void *)hpa,
			(void *)rebased_gpa, size,
			(MMU_MEM_ATTR_READ |
			 MMU_MEM_ATTR_WRITE |
			 MMU_MEM_ATTR_EXECUTE |
			 MMU_MEM_ATTR_WB_CACHE));

	/* Unap gap~gpa+size from sos ept mapping*/
	map_params.pml4_base = vm0->arch_vm.nworld_eptp;
	/* Get the gpa address in SOS */
	gpa = hpa2gpa(vm0, hpa);
	unmap_mem(&map_params, (void *)hpa, (void *)gpa, size, 0);

	/* Backup secure world info, will be used when
	 * destroy secure world */
	vm0->sworld_control.sworld_memory.base_gpa = gpa;
	vm0->sworld_control.sworld_memory.base_hpa = hpa;
	vm0->sworld_control.sworld_memory.length = size;

	mmu_invept(vm->current_vcpu);
	mmu_invept(vm0->current_vcpu);

}

