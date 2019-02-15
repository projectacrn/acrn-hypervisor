/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file guest_memory.h
 *
 * @brief ACRN Memory Management
 */
#ifndef GUEST_H
#define GUEST_H

#ifndef ASSEMBLER

#include <mmu.h>

/* Use # of paging level to identify paging mode */
enum vm_paging_mode {
	PAGING_MODE_0_LEVEL = 0U,	/* Flat */
	PAGING_MODE_2_LEVEL = 2U,	/* 32bit paging, 2-level */
	PAGING_MODE_3_LEVEL = 3U,	/* PAE paging, 3-level */
	PAGING_MODE_4_LEVEL = 4U,	/* 64bit paging, 4-level */
	PAGING_MODE_NUM,
};

/*
 * VM related APIs
 */
int32_t gva2gpa(struct acrn_vcpu *vcpu, uint64_t gva, uint64_t *gpa, uint32_t *err_code);

enum vm_paging_mode get_vcpu_paging_mode(struct acrn_vcpu *vcpu);

/* gpa --> hpa -->hva */
void *gpa2hva(struct acrn_vm *vm, uint64_t x);

uint64_t hva2gpa(struct acrn_vm *vm, void *x);

/**
 * @brief Data transfering between hypervisor and VM
 *
 * @defgroup acrn_mem ACRN Memory Management
 * @{
 */
/**
 * @brief Copy data from VM GPA space to HV address space
 *
 * @param[in] vm The pointer that points to VM data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *                  of HV memory region which data is stored in
 * @param[out] gpa The start GPA address of GPA memory region which data
 *                 will be copied into
 * @param[in] size The size (bytes) of GPA memory region which data is
 *                 stored in
 *
 * @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_from_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);
/**
 * @brief Copy data from HV address space to VM GPA space
 *
 * @param[in] vm The pointer that points to VM data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *                  of HV memory region which data is stored in
 * @param[out] gpa The start GPA address of GPA memory region which data
 *                 will be copied into
 * @param[in] size The size (bytes) of GPA memory region which data will be
 *                 copied into
 *
 * @pre Caller(Guest) should make sure gpa is continuous.
 * - gpa from hypercall input which from kernel stack is gpa continuous, not
 *   support kernel stack from vmap
 * - some other gpa from hypercall parameters, VHM should make sure it's
 *   continuous
 * @pre Pointer vm is non-NULL
 */
int32_t copy_to_gpa(struct acrn_vm *vm, void *h_ptr, uint64_t gpa, uint32_t size);
/**
 * @brief Copy data from VM GVA space to HV address space
 *
 * @param[in] vcpu The pointer that points to vcpu data structure
 * @param[out] h_ptr The pointer that returns the start HV address
 *                   of HV memory region which data will be copied to
 * @param[in] gva The start GVA address of GVA memory region which data
 *                is stored in
 * @param[in] size The size (bytes) of GVA memory region which data is
 *                 stored in
 * @param[out] err_code The page fault flags
 * @param[out] fault_addr The GVA address that causes a page fault
 */
int32_t copy_from_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr);
/**
 * @brief Copy data from HV address space to VM GVA space
 *
 * @param[in] vcpu The pointer that points to vcpu data structure
 * @param[in] h_ptr The pointer that points the start HV address
 *                   of HV memory region which data is stored in
 * @param[out] gva The start GVA address of GVA memory region which data
 *                will be copied into
 * @param[in] size The size (bytes) of GVA memory region which data will
 *                 be copied into
 * @param[out] err_code The page fault flags
 * @param[out] fault_addr The GVA address that causes a page fault
 */
int32_t copy_to_gva(struct acrn_vcpu *vcpu, void *h_ptr, uint64_t gva,
	uint32_t size, uint32_t *err_code, uint64_t *fault_addr);
/**
 * @}
 */
#endif	/* !ASSEMBLER */

#endif /* GUEST_H*/
