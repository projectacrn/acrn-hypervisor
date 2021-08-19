/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <logmsg.h>
#include <asm/cpufeatures.h>
#include <asm/cpuid.h>
#include <asm/rdt.h>
#include <asm/lib/bits.h>
#include <asm/board.h>
#include <asm/vm_config.h>
#include <asm/msr.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vcat.h>
#include <asm/per_cpu.h>

/**
 * @pre vm != NULL
 */
bool is_l2_vcat_configured(const struct acrn_vm *vm)
{
	return is_vcat_configured(vm) && (get_rdt_res_cap_info(RDT_RESOURCE_L2)->num_closids > 0U);
}

/**
 * @pre vm != NULL
 */
bool is_l3_vcat_configured(const struct acrn_vm *vm)
{
	return is_vcat_configured(vm) && (get_rdt_res_cap_info(RDT_RESOURCE_L3)->num_closids > 0U);
}

/**
 * @brief Return number of vCLOSIDs of vm
 *
 * @pre vm != NULL && vm->vm_id < CONFIG_MAX_VM_NUM
 */
uint16_t vcat_get_num_vclosids(const struct acrn_vm *vm)
{
	uint16_t num_vclosids = 0U;

	if (is_vcat_configured(vm)) {
		/*
		 * For performance and simplicity, here number of vCLOSIDs (num_vclosids) is set
		 * equal to the number of pCLOSIDs assigned to this VM (get_vm_config(vm->vm_id)->num_pclosids).
		 * But technically, we do not have to make such an assumption. For example,
		 * Hypervisor could implement CLOSID context switch, then number of vCLOSIDs
		 * can be greater than the number of pCLOSIDs assigned. etc.
		 */
		num_vclosids = get_vm_config(vm->vm_id)->num_pclosids;
	}

	return num_vclosids;
}

/**
 * @brief Map vCLOSID to pCLOSID
 *
 * @pre vm != NULL && vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (get_vm_config(vm->vm_id)->pclosids != NULL) && (vclosid < get_vm_config(vm->vm_id)->num_pclosids)
 */
static uint16_t vclosid_to_pclosid(const struct acrn_vm *vm, uint16_t vclosid)
{
	ASSERT(vclosid < vcat_get_num_vclosids(vm), "vclosid is out of range!");

	/*
	 * pclosids points to an array of assigned pCLOSIDs
	 * Use vCLOSID as the index into the pclosids array, returning the corresponding pCLOSID
	 *
	 * Note that write_vcbm() calls vclosid_to_pclosid() indirectly, in write_vcbm(),
	 * the is_l2_vcbm_msr()/is_l3_vcbm_msr() calls ensure that vclosid is always less than
	 * get_vm_config(vm->vm_id)->num_pclosids, so vclosid is always an array index within bound here
	 */
	return get_vm_config(vm->vm_id)->pclosids[vclosid];
}

/**
 * @brief Return max_pcbm of vm
 * @pre vm != NULL && vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre res == RDT_RESOURCE_L2 || res == RDT_RESOURCE_L3
 */
static uint64_t get_max_pcbm(const struct acrn_vm *vm, int res)
{
	/*
	 * max_pcbm/CLOS_MASK is defined in scenario file and is a contiguous bitmask starting
	 * at bit position low (the lowest assigned physical cache way) and ending at position
	 * high (the highest assigned physical cache way, inclusive). As CBM only allows
	 * contiguous '1' combinations, so max_pcbm essentially is a bitmask that selects/covers
	 * all the physical cache ways assigned to the VM.
	 *
	 * For illustrative purpose, here we assume that we have the two functions
	 * GENMASK() and BIT() defined as follows:
	 * GENMASK(high, low): create a contiguous bitmask starting at bit position low and
	 * ending at position high, inclusive.
	 * BIT(n): create a bitmask with bit n set.
	 *
	 * max_pcbm, min_pcbm, max_vcbm, min_vcbm and the relationship between them
	 * can be expressed as:
	 * max_pcbm = GENMASK(high, low)
	 * min_pcbm = BIT(low)
	 *
	 * max_vcbm = GENMASK(high - low, 0)
	 * min_vcbm = BIT(0)
	 * vcbm_len = bitmap_weight(max_pcbm) = high - low + 1
	 *
	 * pcbm to vcbm (mask off the unwanted bits to prevent erroneous mask values):
	 * vcbm = (pcbm & max_pcbm) >> low
	 *
	 * vcbm to pcbm:
	 * pcbm = (vcbm & max_vcbm) << low
	 *
	 * max_pcbm will be mapped to max_vcbm
	 * min_pcbm will be mapped to min_vcbm
	 */
	uint64_t max_pcbm = 0UL;

	if (is_l2_vcat_configured(vm) && (res == RDT_RESOURCE_L2)) {
		max_pcbm = get_vm_config(vm->vm_id)->max_l2_pcbm;
	} else if (is_l3_vcat_configured(vm) && (res == RDT_RESOURCE_L3)) {
		max_pcbm = get_vm_config(vm->vm_id)->max_l3_pcbm;
	}

	return max_pcbm;
}

/**
 * @brief Return vcbm_len of vm
 * @pre vm != NULL
 */
uint16_t vcat_get_vcbm_len(const struct acrn_vm *vm, int res)
{
	return bitmap_weight(get_max_pcbm(vm, res));
}

/**
 * @brief Return max_vcbm of vm
 * @pre vm != NULL
 */
static uint64_t vcat_get_max_vcbm(const struct acrn_vm *vm, int res)
{
	uint64_t max_pcbm = get_max_pcbm(vm, res);
	/* Find the position low (the first bit set) in max_pcbm */
	uint16_t low = ffs64(max_pcbm);

	/* Right shift max_pcbm by low to get max_vcbm */
	return max_pcbm >> low;
}

/**
 * @brief Map pCBM to vCBM
 *
 * @pre vm != NULL
 */
uint64_t vcat_pcbm_to_vcbm(const struct acrn_vm *vm, uint64_t pcbm, int res)
{
	uint64_t max_pcbm = get_max_pcbm(vm, res);

	/* Find the position low (the first bit set) in max_pcbm */
	uint16_t low = ffs64(max_pcbm);

	/* pcbm set bits should only be in the range of [low, high] */
	return (pcbm & max_pcbm) >> low;
}

/**
 * @brief vCBM MSR write handler
 *
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
int32_t write_vcbm(__unused struct acrn_vcpu *vcpu, __unused uint32_t vmsr, __unused uint64_t val)
{
	/*
	 * TODO: this is going to be implemented in a subsequent commit, will perform the following actions:
	 * write vCBM
	 * vmsr to pmsr and vcbm to pcbm
	 * write pCBM
	 */
	return -EACCES;
}

/**
 * @brief vCLOSID MSR write handler
 *
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
int32_t write_vclosid(struct acrn_vcpu *vcpu, uint64_t val)
{
	int32_t ret = -EACCES;

	if (is_vcat_configured(vcpu->vm)) {
		uint32_t vclosid = (uint32_t)((val >> 32U) & 0xFFFFFFFFUL);

		/*
		 * Validity check on val:
		 * Bits 9:0: RMID (always 0 for now)
		 * Bits 31:10: reserved and must be written with zeros
		 * Bits 63:32: vclosid (must be within permitted range)
		 */
		if (((val & 0xFFFFFFFFUL) == 0UL) && (vclosid < (uint32_t)vcat_get_num_vclosids(vcpu->vm))) {
			uint16_t pclosid;

			/* Write the new vCLOSID value */
			vcpu_set_guest_msr(vcpu, MSR_IA32_PQR_ASSOC, val);

			pclosid = vclosid_to_pclosid(vcpu->vm, (uint16_t)vclosid);
			/*
			 * Write the new pCLOSID value to the guest msr area
			 *
			 * The prepare_auto_msr_area() function has already initialized the vcpu->arch.msr_area.
			 * Here we only need to update the vcpu->arch.msr_area.guest[MSR_AREA_IA32_PQR_ASSOC].value field,
			 * all other vcpu->arch.msr_area fields remains unchanged at runtime.
			 */
			vcpu->arch.msr_area.guest[MSR_AREA_IA32_PQR_ASSOC].value = clos2pqr_msr(pclosid);

			ret = 0;
		}
	}

	return ret;
}

/**
 * @brief Initialize vCBM MSRs
 *
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
static void init_vcbms(struct acrn_vcpu *vcpu, int res, uint32_t msr_base)
{
	uint64_t max_vcbm = vcat_get_max_vcbm(vcpu->vm, res);

	if (max_vcbm != 0UL) {
		uint32_t vmsr;
		/* num_vcbm_msrs = num_vclosids */
		uint16_t num_vcbm_msrs = vcat_get_num_vclosids(vcpu->vm);

		/*
		 * For each vCBM MSR, its initial vCBM is set to max_vcbm,
		 * a bitmask with vcbm_len bits (from 0 to vcbm_len - 1, inclusive)
		 * set to 1 and all other bits set to 0.
		 *
		 * As CBM only allows contiguous '1' combinations, so max_vcbm essentially
		 * is a bitmask that selects all the virtual cache ways assigned to the VM.
		 * It covers all the virtual cache ways the guest VM may access, i.e. the
		 * superset bitmask.
		 */
		for (vmsr = msr_base; vmsr < (msr_base + num_vcbm_msrs); vmsr++) {
			/* Write vCBM MSR */
			(void)write_vcbm(vcpu, vmsr, max_vcbm);
		}
	}
}

/**
 * @brief Initialize vCAT MSRs
 *
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
void init_vcat_msrs(struct acrn_vcpu *vcpu)
{
	if (is_vcat_configured(vcpu->vm)) {
		init_vcbms(vcpu, RDT_RESOURCE_L2, MSR_IA32_L2_MASK_BASE);

		init_vcbms(vcpu, RDT_RESOURCE_L3, MSR_IA32_L3_MASK_BASE);

		(void)write_vclosid(vcpu, 0U);
	}
}
