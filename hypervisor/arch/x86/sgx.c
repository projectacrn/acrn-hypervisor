/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <x86/cpufeatures.h>
#include <x86/cpu_caps.h>
#include <x86/sgx.h>
#include <x86/cpuid.h>
#include <x86/guest/vm.h>
#include <logmsg.h>

#define SGX_OPTED_IN (MSR_IA32_FEATURE_CONTROL_SGX_GE | MSR_IA32_FEATURE_CONTROL_LOCK)

/* For the static variables, which are not explicitly initialzed will be inited to 0 */
static int32_t init_sgx_ret = 0;
static struct epc_section pepc_sections[MAX_EPC_SECTIONS];			/* physcial epc sections */
static struct epc_map vm_epc_maps[MAX_EPC_SECTIONS][CONFIG_MAX_VM_NUM];		/* epc resource mapping for VMs */

static int32_t get_epc_section(uint32_t sec_id, uint64_t* base, uint64_t* size)
{
	uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U, type;
	int32_t ret = 0;

	cpuid_subleaf(CPUID_SGX_LEAF, sec_id + CPUID_SGX_EPC_SUBLEAF_BASE, &eax, &ebx, &ecx, &edx);
	type = eax & CPUID_SGX_EPC_TYPE_MASK;
	if (type == CPUID_SGX_EPC_TYPE_VALID) {
		*base = (((uint64_t)ebx & CPUID_SGX_EPC_HIGH_MASK) << 32U) |
			((uint64_t)eax & CPUID_SGX_EPC_LOW_MASK);
		*size =  (((uint64_t)edx & CPUID_SGX_EPC_HIGH_MASK) << 32U) |
			((uint64_t)ecx & CPUID_SGX_EPC_LOW_MASK);
		if (*size != 0UL) {
			pepc_sections[sec_id].base = *base;
			pepc_sections[sec_id].size = *size;
		} else {
			ret = -EINVAL;
		}
	} else if (type == CPUID_SGX_EPC_TYPE_INVALID) {
		/* indicate the end of epc enumeration */
	} else {
		pr_err("%s: unsupport EPC type %u", __func__, type);
		ret = -EINVAL;
	}

	return ret;
}

/* Enumerate physcial EPC resource and partition it according to VM configurations.
 * Build the mappings between HPA and GPA for EPT mapping later.
 * EPC resource partition and mapping relationship will stay unchanged after sgx init.
 */
static int32_t partition_epc(void)
{
	uint16_t vm_id = 0U;
	uint32_t psec_id = 0U, mid = 0U;
	uint64_t psec_addr = 0UL, psec_size = 0UL;
	uint64_t vm_request_size = 0UL, free_size = 0UL, alloc_size;
	struct acrn_vm_config *vm_config;
	int32_t ret = 0;

	while ((psec_id < MAX_EPC_SECTIONS) && (vm_id < CONFIG_MAX_VM_NUM)) {
		if (vm_request_size == 0U) {
			mid = 0U;
			vm_config = get_vm_config(vm_id);
			vm_request_size = vm_config->epc.size;
		}
		if ((free_size == 0UL) && (vm_request_size != 0UL)) {
			ret = get_epc_section(psec_id, &psec_addr, &psec_size);
			free_size = psec_size;
			if ((ret != 0) || (free_size == 0UL)) {
				break;
			}
			psec_id++;
		}
		if (vm_request_size != 0UL) {
			if (vm_request_size <= free_size) {
				alloc_size = vm_request_size;
			} else {
				alloc_size = free_size;
			}
			vm_epc_maps[mid][vm_id].size = alloc_size;
			vm_epc_maps[mid][vm_id].hpa = psec_addr + psec_size - free_size;
			vm_epc_maps[mid][vm_id].gpa = vm_config->epc.base + vm_config->epc.size - vm_request_size;
			vm_request_size -= alloc_size;
			free_size -= alloc_size;
			mid++;
		}
		if (vm_request_size == 0UL) {
			vm_id++;
		}
	}
	if (vm_request_size != 0UL) {
		ret = -ENOMEM;
	}

	return ret;
}

struct epc_section* get_phys_epc(void)
{
	return pepc_sections;
}

struct epc_map* get_epc_mapping(uint16_t vm_id)
{
	return &vm_epc_maps[0][vm_id];
}

int32_t init_sgx(void)
{
	if (pcpu_has_cap(X86_FEATURE_SGX)) {
		if ((msr_read(MSR_IA32_FEATURE_CONTROL) & SGX_OPTED_IN) == SGX_OPTED_IN){
			init_sgx_ret = partition_epc();
			if (init_sgx_ret != 0) {
				pr_err("Please change SGX/PRM setting in BIOS or EPC setting in VM config");
			}
		}
	}

	return init_sgx_ret;
}

bool is_vsgx_supported(uint16_t vm_id)
{
	return ((init_sgx_ret == 0) && (vm_epc_maps[0][vm_id].size != 0U));
}
