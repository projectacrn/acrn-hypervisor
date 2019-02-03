/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EPT_H
#define EPT_H
#include <types.h>

/**
 * Invalid HPA is defined for error checking,
 * according to SDM vol.3A 4.1.4, the maximum
 * host physical address width is 52
 */
#define INVALID_HPA	(0x1UL << 52U)
#define INVALID_GPA	(0x1UL << 52U)
/* External Interfaces */
/**
 * @brief EPT page tables destroy
 *
 * @param[inout] vm the pointer that points to VM data structure
 *
 * @return None
 */
void destroy_ept(struct acrn_vm *vm);
/**
 * @brief Translating from guest-physical address to host-physcial address
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] gpa the specified guest-physical address
 *
 * @retval hpa the host physical address mapping to the \p gpa
 * @retval INVALID_HPA the HPA of parameter gpa is unmapping
 */
uint64_t gpa2hpa(struct acrn_vm *vm, uint64_t gpa);
/**
 * @brief Translating from guest-physical address to host-physcial address
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] gpa the specified guest-physical address
 * @param[out] size the pointer that returns the page size of
 *                  the page in which the gpa is
 *
 * @retval hpa the host physical address mapping to the \p gpa
 * @retval INVALID_HPA the HPA of parameter gpa is unmapping
 */
uint64_t local_gpa2hpa(struct acrn_vm *vm, uint64_t gpa, uint32_t *size);
/**
 * @brief Translating from host-physical address to guest-physical address for SOS_VM
 *
 * @param[in] hpa the specified host-physical address
 *
 * @pre: the gpa and hpa are identical mapping in SOS.
 */
uint64_t sos_vm_hpa2gpa(uint64_t hpa);
/**
 * @brief Guest-physical memory region mapping
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] pml4_page The physical address of The EPTP
 * @param[in] hpa The specified start host physical address of host
 *                physical memory region that GPA will be mapped
 * @param[in] gpa The specified start guest physical address of guest
 *                physical memory region that needs to be mapped
 * @param[in] size The size of guest physical memory region that needs
 *                 to be mapped
 * @param[in] prot_orig The specified memory access right and memory type
 *
 * @return None
 */
void ept_mr_add(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t hpa,
		uint64_t gpa, uint64_t size, uint64_t prot_orig);
/**
 * @brief Guest-physical memory page access right or memory type updating
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] pml4_page The physical address of The EPTP
 * @param[in] gpa The specified start guest physical address of guest
 *            physical memory region whoes mapping needs to be updated
 * @param[in] size The size of guest physical memory region
 * @param[in] prot_set The specified memory access right and memory type
 *                     that will be set
 * @param[in] prot_clr The specified memory access right and memory type
 *                     that will be cleared
 *
 * @return None
 */
void ept_mr_modify(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa,
		uint64_t size, uint64_t prot_set, uint64_t prot_clr);
/**
 * @brief Guest-physical memory region unmapping
 *
 * @param[in] vm the pointer that points to VM data structure
 * @param[in] pml4_page The physical address of The EPTP
 * @param[in] gpa The specified start guest physical address of guest
 *                physical memory region whoes mapping needs to be deleted
 * @param[in] size The size of guest physical memory region
 *
 * @return None
 *
 * @pre [gpa,gpa+size) has been mapped into host physical memory region
 */
void ept_mr_del(struct acrn_vm *vm, uint64_t *pml4_page, uint64_t gpa,
		uint64_t size);

/**
 * @brief EPT misconfiguration handling
 *
 * @param[in] vcpu the pointer that points to vcpu data structure
 *
 * @retval -EINVAL fail to handle the EPT misconfig
 * @retval 0 Success to handle the EPT misconfig
 */
int32_t ept_misconfig_vmexit_handler(__unused struct acrn_vcpu *vcpu);

/**
 * @}
 */
#endif /* EPT_H */
