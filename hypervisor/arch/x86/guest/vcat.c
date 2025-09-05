/*
 * Copyright (C) 2021-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <logmsg.h>
#include <asm/cpufeatures.h>
#include <asm/cpuid.h>
#include <asm/rdt.h>
#include <bits.h>
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
	uint16_t pcpu = ffs64(vm->hw.cpu_affinity);
	const struct rdt_ins *ins = get_rdt_res_ins(RDT_RESOURCE_L2, pcpu);

	return is_vcat_configured(vm) && ins != NULL && (ins->num_closids > 0U);
}

/**
 * @pre vm != NULL
 */
bool is_l3_vcat_configured(const struct acrn_vm *vm)
{
	uint16_t pcpu = ffs64(vm->hw.cpu_affinity);
	const struct rdt_ins *ins = get_rdt_res_ins(RDT_RESOURCE_L3, pcpu);

	return is_vcat_configured(vm) && ins != NULL && (ins->num_closids > 0U);
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
 * @pre vm != NULL
 */
static bool is_l2_vcbm_msr(const struct acrn_vm *vm, uint32_t vmsr)
{
	/* num_vcbm_msrs = num_vclosids */
	uint16_t num_vcbm_msrs = vcat_get_num_vclosids(vm);

	return (is_l2_vcat_configured(vm)
		&& (vmsr >= MSR_IA32_L2_MASK_BASE) && (vmsr < (MSR_IA32_L2_MASK_BASE + num_vcbm_msrs)));
}

/**
 * @pre vm != NULL
 */
static bool is_l3_vcbm_msr(const struct acrn_vm *vm, uint32_t vmsr)
{
	/* num_vcbm_msrs = num_vclosids */
	uint16_t num_vcbm_msrs = vcat_get_num_vclosids(vm);

	return (is_l3_vcat_configured(vm)
		&& (vmsr >= MSR_IA32_L3_MASK_BASE) && (vmsr < (MSR_IA32_L3_MASK_BASE + num_vcbm_msrs)));
}

/**
 * @brief vCBM MSR read handler
 *
 * @pre vcpu != NULL && vcpu->vm != NULL && rval != NULL
 */
int32_t read_vcbm(const struct acrn_vcpu *vcpu, uint32_t vmsr, uint64_t *rval)
{
	int ret = -EACCES;
	struct acrn_vm *vm = vcpu->vm;

	if (is_vcat_configured(vm) && (is_l2_vcbm_msr(vm, vmsr) || is_l3_vcbm_msr(vm, vmsr))) {
		*rval = vcpu_get_guest_msr(vcpu, vmsr);
		ret = 0;
	}

	return ret;
}

/**
 * @brief Map vCBM to pCBM
 *
 * @pre vm != NULL && ((vcbm & vcat_get_max_vcbm(vm, res)) == vcbm)
 */
static uint64_t vcbm_to_pcbm(const struct acrn_vm *vm, uint64_t vcbm, int res)
{
	uint64_t max_pcbm = get_max_pcbm(vm, res);

	/* Find the position low (the first bit set) in max_pcbm */
	uint16_t low = ffs64(max_pcbm);

	return vcbm << low;
}

void get_cache_shift(uint32_t *l2_shift, uint32_t *l3_shift);
/**
 * @pre vcpu != NULL && l2_id != NULL && l3_id != NULL
 */
static void get_cache_id(struct acrn_vcpu *vcpu, uint32_t *l2_id, uint32_t *l3_id)
{
	uint32_t l2_shift, l3_shift;
	uint32_t apicid = vlapic_get_apicid(vcpu_vlapic(vcpu));

	get_cache_shift(&l2_shift, &l3_shift);

	/*
	 * Relationship between APIC ID and cache ID:
	 * Intel SDM Vol 2, CPUID 04H:
	 * EAX: bits 25 - 14: Maximum number of addressable IDs for logical processors sharing this cache.
	 * The nearest power-of-2 integer that is not smaller than (1 + EAX[25:14]) is the number of unique
	 * initial APIC IDs reserved for addressing different logical processors sharing this cache
	 */
	*l2_id = apicid >> l2_shift;
	*l3_id = apicid >> l3_shift;
}

/**
 * @brief Propagate vCBM to other vCPUs that share cache with vcpu
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
static void propagate_vcbm(struct acrn_vcpu *vcpu, uint32_t vmsr, uint64_t val)
{
	uint16_t i;
	struct acrn_vcpu *tmp_vcpu;
	uint32_t l2_id, l3_id;
	struct acrn_vm *vm = vcpu->vm;

	get_cache_id(vcpu, &l2_id, &l3_id);

	/*
	 * Determine which logical processors share an MSR (for instance local
	 * to a core, or shared across multiple cores) by checking if they have the same
	 * L2/L3 cache id
	 */
	foreach_vcpu(i, vm, tmp_vcpu) {
		uint32_t tmp_l2_id, tmp_l3_id;

		get_cache_id(tmp_vcpu, &tmp_l2_id, &tmp_l3_id);

		if ((is_l2_vcbm_msr(vm, vmsr) && (l2_id == tmp_l2_id))
			|| (is_l3_vcbm_msr(vm, vmsr) && (l3_id == tmp_l3_id))) {
			vcpu_set_guest_msr(tmp_vcpu, vmsr, val);
		}
	}
}

/*
 * Check if bitmask is contiguous:
 * All (and only) contiguous '1' combinations are allowed (e.g. FFFFH, 0FF0H, 003CH, etc.)
 */
static bool is_contiguous(uint64_t bitmask)
{
	bool ret = false;

	if (bitmask != 0UL) {
		uint16_t low = ffs64(bitmask);
		uint16_t high = fls64(bitmask);

		if (((2UL << high) - (1UL << low)) == bitmask) {
			ret = true;
		}
	}

	return ret;
}

/**
 * @brief vCBM MSR write handler
 *
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
int32_t write_vcbm(struct acrn_vcpu *vcpu, uint32_t vmsr, uint64_t val)
{
	int ret = -EACCES;
	struct acrn_vm *vm = vcpu->vm;
	int res = -1;
	uint32_t msr_base;

	if (is_vcat_configured(vm)) {
		if (is_l2_vcbm_msr(vm, vmsr)) {
			res = RDT_RESOURCE_L2;
			msr_base = MSR_IA32_L2_MASK_BASE;
		} else if (is_l3_vcbm_msr(vm, vmsr)) {
			res = RDT_RESOURCE_L3;
			msr_base = MSR_IA32_L3_MASK_BASE;
		}
	}

	if (res >= 0) {
		/*
		 * vcbm set bits should only be in the range of [0, vcbm_len) (vcat_get_max_vcbm),
		 * so mask with vcat_get_max_vcbm to prevent erroneous vCBM value
		 */
		uint64_t masked_vcbm = val & vcat_get_max_vcbm(vm, res);

		/*
		 * Validity check on val:
		 * Bits 63:32 of val are reserved and must be written with zeros
		 * (satisfied by the masked_vcbm == val condition)
		 * vCBM must be contiguous
		 */
		if ((masked_vcbm == val) && is_contiguous(val)) {
			uint32_t pmsr;
			uint16_t vclosid;
			uint64_t pcbm, pvalue;

			/*
			 * Write vCBM first:
			 * The L2 mask MSRs are scoped at the same level as the L2 cache (similarly,
			 * the L3 mask MSRs are scoped at the same level as the L3 cache).
			 *
			 * For example, the MSR_IA32_L3_MASK_n MSRs are scoped at socket level, which means if
			 * we program MSR_IA32_L3_MASK_n on one cpu and the same MSR_IA32_L3_MASK_n on all other cpus
			 * of the same socket will also get the change!
			 * Set vcbm to all the vCPUs that share cache with vcpu to mimic this hardware behavior.
			 */
			propagate_vcbm(vcpu, vmsr, val);

			/* Write pCBM: */
			vclosid = (uint16_t)(vmsr - msr_base);
			pmsr = msr_base + (uint32_t)vclosid_to_pclosid(vm, vclosid);
			pcbm = vcbm_to_pcbm(vm, val, res);
			/* Preserve reserved bits, and only set the pCBM bits */
			pvalue = (msr_read(pmsr) & ~get_max_pcbm(vm, res)) | pcbm;
			msr_write(pmsr, pvalue);

			ret = 0;
		}
	}

	return ret;
}

/**
 * @brief vCLOSID MSR read handler
 *
 * @pre vcpu != NULL && vcpu->vm != NULL
 */
int32_t read_vclosid(const struct acrn_vcpu *vcpu, uint64_t *rval)
{
	int ret = -EACCES;

	if (is_vcat_configured(vcpu->vm)) {
		*rval = vcpu_get_guest_msr(vcpu, MSR_IA32_PQR_ASSOC);
		ret = 0;
	}

	return ret;
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
			 * Here we only need to update the vcpu->arch.msr_area.guest[].value field for IA32_PQR_ASSOC,
			 * all other vcpu->arch.msr_area fields remains unchanged at runtime.
			 */
			vcpu->arch.msr_area.guest[vcpu->arch.msr_area.index_of_pqr_assoc].value = clos2pqr_msr(pclosid);

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
