/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#ifdef CONFIG_MTRR_ENABLED

#define MTRR_FIXED_RANGE_ALL_WB ((uint64_t)MTRR_MEM_TYPE_WB \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 8) \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 16) \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 24) \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 32) \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 40) \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 48) \
					| (((uint64_t)MTRR_MEM_TYPE_WB) << 56))

struct fixed_range_mtrr_maps {
	uint32_t msr;
	uint32_t start;
	uint32_t sub_range_size;
};

#define MAX_FIXED_RANGE_ADDR	0x100000
static struct fixed_range_mtrr_maps fixed_mtrr_map[FIXED_RANGE_MTRR_NUM] = {
	{ MSR_IA32_MTRR_FIX64K_00000, 0x0, 0x10000 },
	{ MSR_IA32_MTRR_FIX16K_80000, 0x80000, 0x4000 },
	{ MSR_IA32_MTRR_FIX16K_A0000, 0xA0000, 0x4000 },
	{ MSR_IA32_MTRR_FIX4K_C0000, 0xC0000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_C8000, 0xC8000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_D0000, 0xD0000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_D8000, 0xD8000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_E0000, 0xE0000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_E8000, 0xE8000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_F0000, 0xF0000, 0x1000 },
	{ MSR_IA32_MTRR_FIX4K_F8000, 0xF8000, 0x1000 },
};

int is_fixed_range_mtrr(uint32_t msr)
{
	return (msr >= fixed_mtrr_map[0].msr)
		&& (msr <= fixed_mtrr_map[FIXED_RANGE_MTRR_NUM - 1].msr);
}

static int get_index_of_fixed_mtrr(uint32_t msr)
{
	int i;

	for (i = 0; i < FIXED_RANGE_MTRR_NUM; i++) {
		if (fixed_mtrr_map[i].msr == msr)
			break;
	}
	return i;
}

int get_subrange_size_of_fixed_mtrr(int subrange_id)
{
	return fixed_mtrr_map[subrange_id].sub_range_size;
}

int get_subrange_start_of_fixed_mtrr(int index, int subrange_id)
{
	return (fixed_mtrr_map[index].start + subrange_id *
		get_subrange_size_of_fixed_mtrr(index));
}

int get_subrange_end_of_fixed_mtrr(int index, int subrange_id)
{
	return (get_subrange_start_of_fixed_mtrr(index, subrange_id) +
		get_subrange_size_of_fixed_mtrr(index) - 1);
}

static inline bool is_mtrr_enabled(struct vcpu *vcpu)
{
	return vcpu->mtrr.def_type.bits.enable;
}

static inline bool is_fixed_range_mtrr_enabled(struct vcpu *vcpu)
{
	return (vcpu->mtrr.cap.bits.fix &&
		vcpu->mtrr.def_type.bits.fixed_enable);
}

static inline uint8_t get_default_memory_type(struct vcpu *vcpu)
{
	return (uint8_t)(vcpu->mtrr.def_type.bits.type);
}

void init_mtrr(struct vcpu *vcpu)
{
	union mtrr_cap_reg cap = {0};
	int i;

	/*
	 * We emulate fixed range MTRRs only
	 * And expecting the guests won't write variable MTRRs
	 * since MTRRCap.vcnt is 0
	 */
	vcpu->mtrr.cap.bits.vcnt = 0;
	vcpu->mtrr.cap.bits.fix = 1;
	vcpu->mtrr.def_type.bits.enable = 1;
	vcpu->mtrr.def_type.bits.fixed_enable = 1;
	vcpu->mtrr.def_type.bits.type = MTRR_MEM_TYPE_UC;

	if (is_vm0(vcpu->vm) && cpu_has_cap(X86_FEATURE_MTRR)) {
		cap.value = msr_read(MSR_IA32_MTRR_CAP);
	}

	for (i = 0; i < FIXED_RANGE_MTRR_NUM; i++) {
		if (cap.bits.fix) {
			/*
			 * The system firmware runs in VMX non-root mode on VM0.
			 * In some cases, the firmware needs particular mem type at
			 * certain mmeory locations (e.g. UC for some hardware
			 * registers), so we need to configure EPT according to the
			 * content of physical MTRRs.
			 */
			vcpu->mtrr.fixed_range[i].value = msr_read(fixed_mtrr_map[i].msr);
		} else {
			/*
			 * For non-vm0 EPT, all memory is setup with WB type in EPT,
			 * so we setup fixed range MTRRs accordingly
			 */
			vcpu->mtrr.fixed_range[i].value = MTRR_FIXED_RANGE_ALL_WB;
		}

		pr_dbg("vm%d vcpu%d fixed-range MTRR[%d]: %16llx",
			vcpu->vm->attr.id, vcpu->vcpu_id, i,
			vcpu->mtrr.fixed_range[i].value);
	}
}

void mtrr_wrmsr(struct vcpu *vcpu, uint32_t msr, uint64_t value)
{
	if (msr == MSR_IA32_MTRR_DEF_TYPE) {
		if (vcpu->mtrr.def_type.value != value) {
			vcpu->mtrr.def_type.value = value;
		}
	} else if (is_fixed_range_mtrr(msr))
		vcpu->mtrr.fixed_range[get_index_of_fixed_mtrr(msr)].value = value;
	else
		pr_err("Write to unexpected MSR: 0x%x", msr);
}

uint64_t mtrr_rdmsr(struct vcpu *vcpu, uint32_t msr)
{
	struct mtrr_state *mtrr = &vcpu->mtrr;
	uint64_t ret = 0;

	if (msr == MSR_IA32_MTRR_CAP)
		ret = mtrr->cap.value;
	else if (msr == MSR_IA32_MTRR_DEF_TYPE)
		ret = mtrr->def_type.value;
	else if (is_fixed_range_mtrr(msr))
		ret = mtrr->fixed_range[get_index_of_fixed_mtrr(msr)].value;
	else
		pr_err("read unexpected MSR: 0x%x", msr);

	return ret;
}

#endif /* CONFIG_MTRR_ENABLED */
