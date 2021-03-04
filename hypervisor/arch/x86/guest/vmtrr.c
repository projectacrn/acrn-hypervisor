/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/guest/vmtrr.h>
#include <x86/msr.h>
#include <x86/pgtable.h>
#include <x86/guest/ept.h>
#include <x86/guest/vcpu.h>
#include <x86/guest/vm.h>
#include <logmsg.h>

#define MTRR_FIXED_RANGE_ALL_WB (MTRR_MEM_TYPE_WB \
					| (MTRR_MEM_TYPE_WB << 8U) \
					| (MTRR_MEM_TYPE_WB << 16U) \
					| (MTRR_MEM_TYPE_WB << 24U) \
					| (MTRR_MEM_TYPE_WB << 32U) \
					| (MTRR_MEM_TYPE_WB << 40U) \
					| (MTRR_MEM_TYPE_WB << 48U) \
					| (MTRR_MEM_TYPE_WB << 56U))

struct fixed_range_mtrr_maps {
	uint32_t msr;
	uint32_t start;
	uint32_t sub_range_size;
};

#define MAX_FIXED_RANGE_ADDR	0x100000UL
#define FIXED_MTRR_INVALID_INDEX	~0U
static struct fixed_range_mtrr_maps fixed_mtrr_map[FIXED_RANGE_MTRR_NUM] = {
	{ MSR_IA32_MTRR_FIX64K_00000, 0x0U, 0x10000U },
	{ MSR_IA32_MTRR_FIX16K_80000, 0x80000U, 0x4000U },
	{ MSR_IA32_MTRR_FIX16K_A0000, 0xA0000U, 0x4000U },
	{ MSR_IA32_MTRR_FIX4K_C0000, 0xC0000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_C8000, 0xC8000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_D0000, 0xD0000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_D8000, 0xD8000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_E0000, 0xE0000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_E8000, 0xE8000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_F0000, 0xF0000U, 0x1000U },
	{ MSR_IA32_MTRR_FIX4K_F8000, 0xF8000U, 0x1000U },
};

static inline struct acrn_vcpu *vmtrr2vcpu(const struct acrn_vmtrr *vmtrr)
{
	return container_of(container_of(vmtrr, struct acrn_vcpu_arch, vmtrr), struct acrn_vcpu, arch);
}

static uint32_t get_index_of_fixed_mtrr(uint32_t msr)
{
	uint32_t i;

	for (i = 0U; i < FIXED_RANGE_MTRR_NUM; i++) {
		if (fixed_mtrr_map[i].msr == msr) {
			break;
		}
	}

	return (i < FIXED_RANGE_MTRR_NUM) ? i : FIXED_MTRR_INVALID_INDEX;
}

static uint32_t
get_subrange_size_of_fixed_mtrr(uint32_t subrange_id)
{
	return fixed_mtrr_map[subrange_id].sub_range_size;
}

static uint32_t
get_subrange_start_of_fixed_mtrr(uint32_t index, uint32_t subrange_id)
{
	return (fixed_mtrr_map[index].start + subrange_id *
		get_subrange_size_of_fixed_mtrr(index));
}

static inline bool is_mtrr_enabled(const struct acrn_vmtrr *vmtrr)
{
	return (vmtrr->def_type.bits.enable != 0U);
}

static inline bool is_fixed_range_mtrr_enabled(const struct acrn_vmtrr *vmtrr)
{
	return ((vmtrr->cap.bits.fix != 0U) &&
		(vmtrr->def_type.bits.fixed_enable != 0U));
}

static inline uint8_t get_default_memory_type(const struct acrn_vmtrr *vmtrr)
{
	return (uint8_t)(vmtrr->def_type.bits.type);
}

/* initialize virtual MTRR for particular vcpu */
void init_vmtrr(struct acrn_vcpu *vcpu)
{
	struct acrn_vmtrr *vmtrr = &vcpu->arch.vmtrr;
	union mtrr_cap_reg cap = {0};
	uint32_t i;

	/*
	 * We emulate fixed range MTRRs only
	 * And expecting the guests won't write variable MTRRs
	 * since MTRRCap.vcnt is 0
	 */
	vmtrr->cap.bits.vcnt = 0U;
	vmtrr->cap.bits.fix = 1U;
	vmtrr->def_type.bits.enable = 1U;
	vmtrr->def_type.bits.fixed_enable = 1U;
	vmtrr->def_type.bits.type = MTRR_MEM_TYPE_UC;

	if (is_sos_vm(vcpu->vm)) {
		cap.value = msr_read(MSR_IA32_MTRR_CAP);
	}

	for (i = 0U; i < FIXED_RANGE_MTRR_NUM; i++) {
		if (cap.bits.fix != 0U) {
			/*
			 * The system firmware runs in VMX non-root mode on SOS_VM.
			 * In some cases, the firmware needs particular mem type
			 * at certain mmeory locations (e.g. UC for some
			 * hardware registers), so we need to configure EPT
			 * according to the content of physical MTRRs.
			 */
			vmtrr->fixed_range[i].value = msr_read(fixed_mtrr_map[i].msr);
		} else {
			/*
			 * For non-sos_vm EPT, all memory is setup with WB type in
			 * EPT, so we setup fixed range MTRRs accordingly.
			 */
			vmtrr->fixed_range[i].value = MTRR_FIXED_RANGE_ALL_WB;
		}

		pr_dbg("vm%d vcpu%hu fixed-range MTRR[%u]: %16lx",
			vcpu->vm->vm_id, vcpu->vcpu_id, i,
			vmtrr->fixed_range[i].value);
	}
}

static void update_ept(struct acrn_vm *vm, uint64_t start,
	uint64_t size, uint8_t type)
{
	uint64_t attr;

	switch ((uint64_t)type) {
	case MTRR_MEM_TYPE_WC:
		attr = EPT_WC;
		break;
	case MTRR_MEM_TYPE_WT:
		attr = EPT_WT;
		break;
	case MTRR_MEM_TYPE_WP:
		attr = EPT_WP;
		break;
	case MTRR_MEM_TYPE_WB:
		attr = EPT_WB;
		break;
	case MTRR_MEM_TYPE_UC:
	default:
		attr = EPT_UNCACHED;
		break;
	}

	ept_modify_mr(vm, (uint64_t *)vm->arch_vm.nworld_eptp, start, size, attr, EPT_MT_MASK);
}

static void update_ept_mem_type(const struct acrn_vmtrr *vmtrr)
{
	uint8_t type;
	uint64_t start, size;
	uint32_t i, j;
	struct acrn_vm *vm = vmtrr2vcpu(vmtrr)->vm;

	/*
	 * Intel SDM, Vol 3, 11.11.2.1 Section "IA32_MTRR_DEF_TYPE MSR":
	 * - when def_type.E is clear, UC memory type is applied
	 * - when def_type.FE is clear, MTRRdefType.type is applied
	 */
	if (!is_mtrr_enabled(vmtrr) || !is_fixed_range_mtrr_enabled(vmtrr)) {
		update_ept(vm, 0U, MAX_FIXED_RANGE_ADDR, get_default_memory_type(vmtrr));
	} else {
		/* Deal with fixed-range MTRRs only */
		for (i = 0U; i < FIXED_RANGE_MTRR_NUM; i++) {
			type = vmtrr->fixed_range[i].type[0];
			start = get_subrange_start_of_fixed_mtrr(i, 0U);
			size = get_subrange_size_of_fixed_mtrr(i);

			for (j = 1U; j < MTRR_SUB_RANGE_NUM; j++) {
				/* If it's same type, combine the subrange together */
				if (type == vmtrr->fixed_range[i].type[j]) {
					size += get_subrange_size_of_fixed_mtrr(i);
				} else {
					update_ept(vm, start, size, type);
					type = vmtrr->fixed_range[i].type[j];
					start = get_subrange_start_of_fixed_mtrr(i, j);
					size = get_subrange_size_of_fixed_mtrr(i);
				}
			}

			update_ept(vm, start, size, type);
		}
	}
}

/* virtual MTRR MSR write API */
void write_vmtrr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t value)
{
	struct acrn_vmtrr *vmtrr = &vcpu->arch.vmtrr;
	uint32_t index;

	if (msr == MSR_IA32_MTRR_DEF_TYPE) {
		if (vmtrr->def_type.value != value) {
			vmtrr->def_type.value = value;

			/*
			 * Guests follow this guide line to update MTRRs:
			 * Intel SDM, Volume 3, 11.11.8 Section "MTRR
			 * Considerations in MP Systems"
			 * 1. Broadcast to all processors
			 * 2. Disable Interrupts
			 * 3. Wait for all procs to do so
			 * 4. Enter no-fill cache mode (CR0.CD=1, CR0.NW=0)
			 * 5. Flush caches
			 * 6. Clear CR4.PGE bit
			 * 7. Flush all TLBs
			 * 8. Disable all range registers by MTRRdefType.E
			 * 9. Update the MTRRs
			 * 10. Enable all range registers by MTRRdeftype.E
			 * 11. Flush all TLBs and caches again
			 * 12. Enter normal cache mode to re-enable caching
			 * 13. Set CR4.PGE
			 * 14. Wait for all processors to reach this point
			 * 15. Enable interrupts.
			 *
			 * we don't have to update EPT in step 9
			 * but in step 8 and 10 only
			 */
			update_ept_mem_type(vmtrr);
		}
	} else {
		index = get_index_of_fixed_mtrr(msr);
		if (index != FIXED_MTRR_INVALID_INDEX) {
			vmtrr->fixed_range[index].value = value;
		} else {
			pr_err("Write to unexpected MSR: 0x%x", msr);
		}
	}
}

/* virtual MTRR MSR read API */
uint64_t read_vmtrr(const struct acrn_vcpu *vcpu, uint32_t msr)
{
	const struct acrn_vmtrr *vmtrr = &vcpu->arch.vmtrr;
	uint64_t ret = 0UL;
	uint32_t index;

	if (msr == MSR_IA32_MTRR_CAP) {
		ret = vmtrr->cap.value;
	} else if (msr == MSR_IA32_MTRR_DEF_TYPE) {
		ret = vmtrr->def_type.value;
	} else {
		index = get_index_of_fixed_mtrr(msr);
		if (index != FIXED_MTRR_INVALID_INDEX) {
			ret = vmtrr->fixed_range[index].value;
		} else {
			pr_err("read unexpected MSR: 0x%x", msr);
		}
	}

	return ret;
}
