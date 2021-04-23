/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SGX_H
#define SGX_H

#define CPUID_SGX_LEAF				0x12U
#define CPUID_SGX_EPC_SUBLEAF_BASE 		0x2U
#define CPUID_SGX_EPC_TYPE_MASK			0xFU
#define CPUID_SGX_EPC_TYPE_INVALID		0x0U
#define CPUID_SGX_EPC_TYPE_VALID		0x1U
#define CPUID_SGX_EPC_HIGH_MASK			0x000FFFFFU
#define CPUID_SGX_EPC_LOW_MASK			0xFFFFF000U

#define MAX_EPC_SECTIONS			4U
/**
 * @file sgx.h
 *
 * @brief public APIs for SGX
 */

/**
 * @brief SGX
 *
 * @defgroup acrn_sgx ACRN SGX
 * @{
 */

struct epc_section
{
	uint64_t base;	/* EPC section base, must be page aligned */
	uint64_t size;  /* EPC section size in byte, must be page aligned */
};

struct epc_map
{
	uint64_t hpa;		/* EPC reource address in host, must be page aligned */
	uint64_t gpa;		/* EPC reource address in guest, must be page aligned */
	uint64_t size;		/* EPC reource size in byte, must be page aligned */
};

/**
 * @brief Get physcial EPC sections of the platform.
 *
 * @retval Physical EPC sections of the platform
 *
 */
struct epc_section* get_phys_epc(void);

/**
 * @brief Get EPC resource information for a specific VM.
 *
 * @param[in] vm_id VM ID to specify a VM
 *
 * @retval EPC sections for a VM
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 *
 */
struct epc_map* get_epc_mapping(uint16_t vm_id);

/**
 * @brief If SGX support is enabled or not for a specific VM.
 *
 * @param[in] vm_id VM ID to specify a VM
 *
 * @retval True when SGX is supported in the specific VM
 * @retval False When SGX is not supported in the specific VM
 *
 * @pre vm_id < CONFIG_MAX_VM_NUM
 *
 */
bool is_vsgx_supported(uint16_t vm_id);

/**
 * @brief SGX initialization.
 *
 * Init SGX and parition EPC resource for VMs.
 *
 * @retval 0 on success
 * @retval <0 on failure
 *
 */
int32_t init_sgx(void);
/**
  * @}
  */

#endif
