/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#include "guest/instr_emul_wrapper.h"
#include "guest/instr_emul.h"

#define ACRN_DBG_EPT	6U


static uint64_t find_next_table(uint32_t table_offset, void *table_base)
{
	uint64_t table_entry;
	uint64_t table_present;
	uint64_t sub_table_addr = 0UL;

	/* Read the table entry */
	table_entry = mem_read64(table_base
			+ (table_offset * IA32E_COMM_ENTRY_SIZE));

	/* If bit 7 is set, entry is not a subtable. */
	if ((table_entry & IA32E_PDPTE_PS_BIT) != 0U
	    || (table_entry & IA32E_PDE_PS_BIT) != 0U) {
		return sub_table_addr;
	}

	/* Set table present bits to any of the read/write/execute bits */
	table_present = (IA32E_EPT_R_BIT | IA32E_EPT_W_BIT | IA32E_EPT_X_BIT);

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

static void free_ept_mem(void *pml4_addr)
{
	void *pdpt_addr;
	void *pde_addr;
	void *pte_addr;
	uint32_t pml4_index;
	uint32_t pdpt_index;
	uint32_t pde_index;

	if (pml4_addr == NULL) {
		ASSERT(false, "EPTP is NULL");
		return;
	}

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

			for (pde_index = 0U; pde_index < IA32E_NUM_ENTRIES;
					pde_index++) {
				/* Walk from the PD table to the page table */
				pte_addr = HPA2HVA(find_next_table(pde_index,
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
	free_ept_mem(HPA2HVA(vm->arch_vm.nworld_eptp));
	free_ept_mem(HPA2HVA(vm->arch_vm.m2p));

	/*
	 * If secure world is initialized, destroy Secure world ept.
	 * There are two cases secure world is not initialized:
	 *  - trusty is not enabled. Check sworld_enabled.
	 *  - trusty is enabled. But not initialized yet.
	 *    Check vm->arch_vm.sworld_eptp.
	 */
	if (vm->sworld_control.sworld_enabled && (vm->arch_vm.sworld_eptp != 0U)) {
		free_ept_mem(HPA2HVA(vm->arch_vm.sworld_eptp));
		vm->arch_vm.sworld_eptp = 0UL;
	}
}

uint64_t _gpa2hpa(struct vm *vm, uint64_t gpa, uint32_t *size)
{
	uint64_t hpa = 0UL;
	uint32_t pg_size = 0U;
	struct entry_params entry;
	struct map_params map_params;

	map_params.page_table_type = PTT_EPT;
	map_params.pml4_base = HPA2HVA(vm->arch_vm.nworld_eptp);
	map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);
	obtain_last_page_table_entry(&map_params, &entry, (void *)gpa, true);
	if (entry.entry_present == PT_PRESENT) {
		hpa = ((entry.entry_val & (~(entry.page_size - 1)))
				| (gpa & (entry.page_size - 1)));
		pg_size = entry.page_size;
		pr_dbg("GPA2HPA: 0x%llx->0x%llx", gpa, hpa);
	} else {
		pr_err("VM %d GPA2HPA: failed for gpa 0x%llx",
				vm->attr.boot_idx, gpa);
	}

	if (size != NULL) {
		*size = pg_size;
	}

	return hpa;
}

/* using return value 0 as failure, make sure guest will not use hpa 0 */
uint64_t gpa2hpa(struct vm *vm, uint64_t gpa)
{
	return _gpa2hpa(vm, gpa, NULL);
}

uint64_t hpa2gpa(struct vm *vm, uint64_t hpa)
{
	struct entry_params entry;
	struct map_params map_params;

	map_params.page_table_type = PTT_EPT;
	map_params.pml4_base = HPA2HVA(vm->arch_vm.nworld_eptp);
	map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);

	obtain_last_page_table_entry(&map_params, &entry,
			(void *)hpa, false);

	if (entry.entry_present == PT_NOT_PRESENT) {
		pr_err("VM %d hpa2gpa: failed for hpa 0x%llx",
				vm->attr.boot_idx, hpa);
		ASSERT(false, "hpa2gpa not found");
	}
	return ((entry.entry_val & (~(entry.page_size - 1)))
			| (hpa & (entry.page_size - 1)));
}

bool is_ept_supported(void)
{
	bool status;
	uint64_t tmp64;

	/* Read primary processor based VM control. */
	tmp64 = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);

	/* Check if secondary processor based VM control is available. */
	if ((tmp64 & MMU_MEM_ATTR_BIT_EXECUTE_DISABLE) != 0U) {
		/* Read primary processor based VM control. */
		tmp64 = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);

		/* Check if EPT is supported. */
		if ((tmp64 & (((uint64_t)VMX_PROCBASED_CTLS2_EPT) << 32)) != 0U) {
			/* EPT is present. */
			status = true;
		} else {
			status = false;
		}

	} else {
		/* Secondary processor based VM control is not present */
		status = false;
	}

	return status;
}

static int hv_emulate_mmio(struct vcpu *vcpu, struct mem_io *mmio,
				struct mem_io_node *mmio_handler)
{
	if ((mmio->paddr % mmio->access_size) != 0) {
		pr_err("access size not align with paddr");
		return -EINVAL;
	}

	/* Handle this MMIO operation */
	return mmio_handler->read_write(vcpu, mmio,
			mmio_handler->handler_private_data);
}

int register_mmio_emulation_handler(struct vm *vm,
	hv_mem_io_handler_t read_write, uint64_t start,
	uint64_t end, void *handler_private_data)
{
	int status = -EINVAL;
	struct mem_io_node *mmio_node;

	if (vm->hw.created_vcpus > 0 && vm->hw.vcpu_array[0]->launched) {
		ASSERT(false, "register mmio handler after vm launched");
		return status;
	}

	/* Ensure both a read/write handler and range check function exist */
	if ((read_write != NULL) && (end > start)) {
		/* Allocate memory for node */
		mmio_node =
		(struct mem_io_node *)calloc(1U, sizeof(struct mem_io_node));

		/* Ensure memory successfully allocated */
		if (mmio_node != NULL) {
			/* Fill in information for this node */
			mmio_node->read_write = read_write;
			mmio_node->handler_private_data = handler_private_data;

			INIT_LIST_HEAD(&mmio_node->list);
			list_add(&mmio_node->list, &vm->mmio_list);

			mmio_node->range_start = start;
			mmio_node->range_end = end;

			/*
			 * SOS would map all its memory at beginning, so we
			 * should unmap it. But UOS will not, so we shouldn't
			 * need to unmap it.
			 */
			if (is_vm0(vm)) {
				ept_mmap(vm, start, start, end - start,
					MAP_UNMAP, 0);
			}

			/* Return success */
			status = 0;
		}
	}

	/* Return status to caller */
	return status;
}

void unregister_mmio_emulation_handler(struct vm *vm, uint64_t start,
	uint64_t end)
{
	struct list_head *pos, *tmp;
	struct mem_io_node *mmio_node;

	list_for_each_safe(pos, tmp, &vm->mmio_list) {
		mmio_node = list_entry(pos, struct mem_io_node, list);

		if ((mmio_node->range_start == start) &&
			(mmio_node->range_end == end)) {
			/* assume only one entry found in mmio_list */
			list_del_init(&mmio_node->list);
			free(mmio_node);
			break;
		}
	}
}

int dm_emulate_mmio_post(struct vcpu *vcpu)
{
	int ret = 0;
	uint16_t cur = vcpu->vcpu_id;
	union vhm_request_buffer *req_buf;

	req_buf = (union vhm_request_buffer *)(vcpu->vm->sw.io_shared_page);

	vcpu->req.reqs.mmio_request.value =
		req_buf->req_queue[cur].reqs.mmio_request.value;

	/* VHM emulation data already copy to req, mark to free slot now */
	req_buf->req_queue[cur].valid = false;

	if (req_buf->req_queue[cur].processed == REQ_STATE_SUCCESS) {
		vcpu->mmio.mmio_status = MMIO_TRANS_VALID;
	}
	else {
		vcpu->mmio.mmio_status = MMIO_TRANS_INVALID;
		goto out;
	}

	if (vcpu->mmio.read_write == HV_MEM_IO_READ) {
		vcpu->mmio.value = vcpu->req.reqs.mmio_request.value;
		/* Emulate instruction and update vcpu register set */
		ret = emulate_instruction(vcpu);
		if (ret != 0) {
			goto out;
		}
	}

out:
	return ret;
}

static int dm_emulate_mmio_pre(struct vcpu *vcpu, uint64_t exit_qual)
{
	int status;

	if (vcpu->mmio.read_write == HV_MEM_IO_WRITE) {
		status = emulate_instruction(vcpu);
		if (status != 0) {
			return status;
		}
		vcpu->req.reqs.mmio_request.value = vcpu->mmio.value;
		/* XXX: write access while EPT perm RX -> WP */
		if ((exit_qual & 0x38UL) == 0x28UL) {
			vcpu->req.type = REQ_WP;
		}
	}

	if (vcpu->req.type == 0U) {
		vcpu->req.type = REQ_MMIO;
	}
	vcpu->req.reqs.mmio_request.direction = vcpu->mmio.read_write;
	vcpu->req.reqs.mmio_request.address = (long)vcpu->mmio.paddr;
	vcpu->req.reqs.mmio_request.size = vcpu->mmio.access_size;

	return 0;
}

int ept_violation_vmexit_handler(struct vcpu *vcpu)
{
	int status = -EINVAL, ret;
	uint64_t exit_qual;
	uint64_t gpa;
	struct list_head *pos;
	struct mem_io *mmio = &vcpu->mmio;
	struct mem_io_node *mmio_handler = NULL;

	/* Handle page fault from guest */
	exit_qual = vcpu->arch_vcpu.exit_qualification;

	/* Specify if read or write operation */
	if ((exit_qual & 0x2UL) != 0UL) {
		/* Write operation */
		mmio->read_write = HV_MEM_IO_WRITE;

		/* Get write value from appropriate register in context */
		/* TODO: Need to figure out how to determine value being
		 * written
		 */
		mmio->value = 0UL;
	} else {
		/* Read operation */
		mmio->read_write = HV_MEM_IO_READ;

		/* Get sign extension requirements for read */
		/* TODO: Need to determine how sign extension is determined for
		 * reads
		 */
		mmio->sign_extend_read = 0U;
	}

	/* Get the guest physical address */
	gpa = exec_vmread64(VMX_GUEST_PHYSICAL_ADDR_FULL);

	TRACE_2L(TRACE_VMEXIT_EPT_VIOLATION, exit_qual, gpa);

	/* Adjust IPA appropriately and OR page offset to get full IPA of abort
	 */
	mmio->paddr = gpa;

	ret = decode_instruction(vcpu);
	if (ret > 0) {
		mmio->access_size = ret;
	}
	else if (ret == -EFAULT) {
		pr_info("page fault happen during decode_instruction");
		status = 0;
		goto out;
	}
	else {
		goto out;
	}

	list_for_each(pos, &vcpu->vm->mmio_list) {
		mmio_handler = list_entry(pos, struct mem_io_node, list);
		if ((mmio->paddr + mmio->access_size <=
			mmio_handler->range_start) ||
			(mmio->paddr >= mmio_handler->range_end)) {
			continue;
		}
		else if (!((mmio->paddr >= mmio_handler->range_start) &&
			(mmio->paddr + mmio->access_size <=
			mmio_handler->range_end))) {
			pr_fatal("Err MMIO, addr:0x%llx, size:%x",
					mmio->paddr, mmio->access_size);
			return -EIO;
		}

		if (mmio->read_write == HV_MEM_IO_WRITE) {
			if (emulate_instruction(vcpu) != 0) {
				goto out;
			}
		}

		/* Call generic memory emulation handler
		 * For MMIO write, call hv_emulate_mmio after
		 * instruction emulation. For MMIO read,
		 * call hv_emulate_mmio at first.
		 */
		hv_emulate_mmio(vcpu, mmio, mmio_handler);
		if (mmio->read_write == HV_MEM_IO_READ) {
			/* Emulate instruction and update vcpu register set */
			if (emulate_instruction(vcpu) != 0) {
				goto out;
			}
		}

		status = 0;
		break;
	}

	if (status != 0) {
		/*
		 * No mmio handler from HV side, search from VHM in Dom0
		 *
		 * ACRN insert request to VHM and inject upcall
		 * For MMIO write, ask DM to run MMIO emulation after
		 * instruction emulation. For MMIO read, ask DM to run MMIO
		 * emulation at first.
		 */
		(void)memset(&vcpu->req, 0, sizeof(struct vhm_request));

		if (dm_emulate_mmio_pre(vcpu, exit_qual) != 0) {
			goto out;
		}

		status = acrn_insert_request_wait(vcpu, &vcpu->req);
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

int ept_mmap(struct vm *vm, uint64_t hpa,
	uint64_t gpa, uint64_t size, uint32_t type, uint32_t prot)
{
	struct map_params map_params;
	uint16_t i;
	struct vcpu *vcpu;

	/* Setup memory map parameters */
	map_params.page_table_type = PTT_EPT;
	if (vm->arch_vm.nworld_eptp != 0U) {
		map_params.pml4_base = HPA2HVA(vm->arch_vm.nworld_eptp);
		map_params.pml4_inverted = HPA2HVA(vm->arch_vm.m2p);
	} else {
		map_params.pml4_base = alloc_paging_struct();
		vm->arch_vm.nworld_eptp = HVA2HPA(map_params.pml4_base);
		map_params.pml4_inverted = alloc_paging_struct();
		vm->arch_vm.m2p = HVA2HPA(map_params.pml4_inverted);
	}

	if (type == MAP_MEM || type == MAP_MMIO) {
		/* EPT & VT-d share the same page tables, set SNP bit
		 * to force snooping of PCIe devices if the page
		 * is cachable
		 */
		if ((prot & IA32E_EPT_MT_MASK) != IA32E_EPT_UNCACHED) {
			prot |= IA32E_EPT_SNOOP_CTRL;
		}
		map_mem(&map_params, (void *)hpa,
			(void *)gpa, size, prot);

	} else if (type == MAP_UNMAP) {
		unmap_mem(&map_params, (void *)hpa, (void *)gpa,
				size, prot);
	} else {
		ASSERT(false, "unknown map type");
	}

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	dev_dbg(ACRN_DBG_EPT, "ept map: %s hpa: 0x%016llx gpa: 0x%016llx ",
			type == MAP_UNMAP ? "unmap" : "map", hpa, gpa);
	dev_dbg(ACRN_DBG_EPT, "size: 0x%016llx prot: 0x%x\n", size, prot);

	return 0;
}

int ept_mr_modify(struct vm *vm, uint64_t gpa, uint64_t size,
		uint64_t attr_set, uint64_t attr_clr)
{
	struct vcpu *vcpu;
	uint16_t i;
	int ret;

	ret = mmu_modify((uint64_t *)HPA2HVA(vm->arch_vm.nworld_eptp),
			gpa, size, attr_set, attr_clr, PTT_EPT);

	foreach_vcpu(i, vm, vcpu) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	return ret;
}
