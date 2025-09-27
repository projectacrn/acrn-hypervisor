/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <vcpu.h>
#include <vm.h>
#include <guest_memory.h>

#include <pgtable.h>

#define INVALID_HPA (0x1UL << 52U)
#define INVALID_GPA (0x1UL << 52U)

void dummy_pgtable_add_map(uint64_t paddr_base, uint64_t vaddr_base, uint64_t size, uint64_t prot);

int32_t gva2gpa(struct acrn_vcpu *vcpu, uint64_t gva, uint64_t *gpa, uint32_t *err_code)
{
	/* TODO: dummy implementation, assuming gva 1-1 mapping to gpa */
	(void)vcpu;
	(void)err_code;
	*gpa = gva;
	return 0;
}

uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa)
{
	/* TODO: dummy implementation, assuming 1-1 gpa2hpa mapping */
	(void)vm;
	return gpa;
}

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu)
{
	(void)vcpu;
	return PAGING_MODE_4_LEVEL;
}

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x)
{
	uint64_t hpa = gpa2hpa(vm, x);
	return (hpa == INVALID_HPA) ? NULL : hpa2hva(hpa);
}

int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	/* TODO: dummy implementation */
	(void)vm;
	uint64_t hpa = hva2hpa(h_ptr);
	dummy_pgtable_add_map(hpa, hpa, size, PAGE_V | PAGE_X | PAGE_R | PAGE_W);
	dummy_pgtable_add_map(gpa, gpa, size, PAGE_V | PAGE_X | PAGE_R | PAGE_W);
	memcpy(h_ptr, (void *)gpa, size);
	return 0;
}

int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size)
{
	/* TODO: dummy implementation */
	(void)vm;
	uint64_t hpa = hva2hpa(h_ptr);
	dummy_pgtable_add_map(hpa, hpa, size, PAGE_V | PAGE_X | PAGE_R | PAGE_W);
	dummy_pgtable_add_map(gpa, gpa, size, PAGE_V | PAGE_X | PAGE_R | PAGE_W);
	memcpy((void *)gpa, h_ptr, size);
	return 0;
}

int32_t copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	/* TODO: to be implemented */
	(void)vcpu; (void)h_ptr; (void)gva; (void)size; (void)err_code; (void)fault_addr;
	return 0;
}

int32_t copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr)
{
	/* TODO: to be implemented */
	(void)vcpu; (void)h_ptr; (void)gva; (void)size; (void)err_code; (void)fault_addr;
	return 0;
}
