/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/cpuid.h>
#include <asm/cpufeatures.h>
#include <asm/vmx.h>
#include <asm/sgx.h>
#include <asm/tsc.h>
#include <logmsg.h>
#include <asm/rdt.h>
#include <asm/guest/vcat.h>

static struct percpu_cpuids {
	uint32_t leaf_nr;
	uint32_t leaves[MAX_VM_VCPUID_ENTRIES];
} pcpu_cpuids;

static inline const struct vcpuid_entry *local_find_vcpuid_entry(const struct acrn_vcpu *vcpu,
					uint32_t leaf, uint32_t subleaf)
{
	uint32_t i = 0U, nr, half;
	const struct vcpuid_entry *found_entry = NULL;
	struct acrn_vm *vm = vcpu->vm;

	nr = vm->vcpuid_entry_nr;
	half = nr >> 1U;
	if (vm->vcpuid_entries[half].leaf < leaf) {
		i = half;
	}

	for (; i < nr; i++) {
		const struct vcpuid_entry *tmp = (const struct vcpuid_entry *)(&vm->vcpuid_entries[i]);

		if (tmp->leaf < leaf) {
			continue;
		} else if (tmp->leaf == leaf) {
			if (((tmp->flags & CPUID_CHECK_SUBLEAF) != 0U) && (tmp->subleaf != subleaf)) {
				continue;
			}
			found_entry = tmp;
			break;
		} else {
			/* tmp->leaf > leaf */
			break;
		}
	}

	return found_entry;
}

static inline const struct vcpuid_entry *find_vcpuid_entry(const struct acrn_vcpu *vcpu,
					uint32_t leaf_arg, uint32_t subleaf)
{
	const struct vcpuid_entry *entry;
	uint32_t leaf = leaf_arg;

	entry = local_find_vcpuid_entry(vcpu, leaf, subleaf);
	if (entry == NULL) {
		uint32_t limit;
		struct acrn_vm *vm = vcpu->vm;

		if ((leaf & 0x80000000U) != 0U) {
			limit = vm->vcpuid_xlevel;
		} else {
			limit = vm->vcpuid_level;
		}

		if (leaf > limit) {
			/* Intel documentation states that invalid EAX input
			 * will return the same information as EAX=cpuid_level
			 * (Intel SDM Vol. 2A - Instruction Set Reference -
			 * CPUID)
			 */
			leaf = vm->vcpuid_level;
			entry = local_find_vcpuid_entry(vcpu, leaf, subleaf);
		}

	}

	return entry;
}

static inline int32_t set_vcpuid_entry(struct acrn_vm *vm,
				const struct vcpuid_entry *entry)
{
	struct vcpuid_entry *tmp;
	size_t entry_size = sizeof(struct vcpuid_entry);
	int32_t ret;

	if (vm->vcpuid_entry_nr == MAX_VM_VCPUID_ENTRIES) {
		pr_err("%s, vcpuid entry over MAX_VM_VCPUID_ENTRIES(%u)\n", __func__, MAX_VM_VCPUID_ENTRIES);
		ret = -ENOMEM;
	} else {
		tmp = &vm->vcpuid_entries[vm->vcpuid_entry_nr];
		vm->vcpuid_entry_nr++;
		(void)memcpy_s(tmp, entry_size, entry, entry_size);
		ret = 0;
	}
	return ret;
}

/**
 * initialization of virtual CPUID leaf
 */
static void init_vcpuid_entry(uint32_t leaf, uint32_t subleaf,
			uint32_t flags, struct vcpuid_entry *entry)
{
	struct cpuinfo_x86 *cpu_info;

	entry->leaf = leaf;
	entry->subleaf = subleaf;
	entry->flags = flags;

	switch (leaf) {
	case 0x16U:
		cpu_info = get_pcpu_info();
		if (cpu_info->cpuid_level >= 0x16U) {
			/* call the cpuid when 0x16 is supported */
			cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		} else {
			/* Use the tsc to derive the emulated 0x16U cpuid. */
			entry->eax = (uint32_t) (get_tsc_khz() / 1000U);
			entry->ebx = entry->eax;
			/* Bus frequency: hard coded to 100M */
			entry->ecx = 100U;
			entry->edx = 0U;
		}
		break;

	/*
	 * Leaf 0x40000000
	 * This leaf returns the CPUID leaf range supported by the
	 * hypervisor and the hypervisor vendor signature.
	 *
	 * EAX: The maximum input value for CPUID supported by the
	 *	hypervisor.
	 * EBX, ECX, EDX: Hypervisor vendor ID signature.
	 */
	case 0x40000000U:
	{
		static const char sig[12] = "ACRNACRNACRN";
		const uint32_t *sigptr = (const uint32_t *)sig;

		entry->eax = 0x40000010U;
		entry->ebx = sigptr[0];
		entry->ecx = sigptr[1];
		entry->edx = sigptr[2];
		break;
	}

	/*
	 * Leaf 0x40000001 - ACRN extended information.
	 * This leaf returns the extended information of ACRN hypervisor.
	 *
	 * EAX: Guest capability flags
	 * EBX, ECX, EDX: RESERVED (reserved fields are set to zero).
	 */
	case 0x40000001U:
		entry->eax = 0U;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;

	/*
	 * Leaf 0x40000010 - Timing Information.
	 * This leaf returns the current TSC frequency and
	 * current Bus frequency in kHz.
	 *
	 * EAX: (Virtual) TSC frequency in kHz.
	 *      TSC frequency is calculated from PIT in ACRN
	 * EBX, ECX, EDX: RESERVED (reserved fields are set to zero).
	 */
	case 0x40000010U:
		entry->eax = get_tsc_khz();
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;

	default:
		cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);
		break;
	}
}

static int32_t set_vcpuid_sgx(struct acrn_vm *vm)
{
	int32_t result = 0;

	if (is_vsgx_supported(vm->vm_id)) {
		struct vcpuid_entry entry;
		struct epc_map *maps;
		uint32_t mid;
		uint64_t size = 0;
		/* init cpuid.12h.0h */
		init_vcpuid_entry(CPUID_SGX_LEAF, 0U, CPUID_CHECK_SUBLEAF, &entry);
		result = set_vcpuid_entry(vm, &entry);
		/* init cpuid.12h.1h */
		if (result == 0) {
			init_vcpuid_entry(CPUID_SGX_LEAF, 1U, CPUID_CHECK_SUBLEAF, &entry);
			/* MPX not present to guest */
			entry.ecx &= (uint32_t) ~XCR0_BNDREGS;
			entry.ecx &= (uint32_t) ~XCR0_BNDCSR;
			result = set_vcpuid_entry(vm, &entry);
		}
		if (result == 0) {
			maps = get_epc_mapping(vm->vm_id);
			/* init cpuid.12h.2h, only build one EPC section for a VM */
			for (mid = 0U; mid <= MAX_EPC_SECTIONS; mid++) {
				size += maps[mid].size;
			}
			entry.leaf = CPUID_SGX_LEAF;
			entry.subleaf = CPUID_SGX_EPC_SUBLEAF_BASE;
			entry.flags = CPUID_CHECK_SUBLEAF;
			entry.eax = CPUID_SGX_EPC_TYPE_VALID;
			entry.eax |= (uint32_t)maps[0].gpa & CPUID_SGX_EPC_LOW_MASK;
			entry.ebx = (uint32_t)(maps[0].gpa >> 32U) &  CPUID_SGX_EPC_HIGH_MASK;
			entry.ecx = (uint32_t)size & CPUID_SGX_EPC_LOW_MASK;
			entry.edx = (uint32_t)(size >> 32U) & CPUID_SGX_EPC_HIGH_MASK;
			result = set_vcpuid_entry(vm, &entry);
		}
	}

	return result;
}

#ifdef CONFIG_VCAT_ENABLED
/**
 * @brief * Number of ways (CBM length) is detected with CPUID.0x4
 *
 * @pre vm != NULL
 */
static int32_t set_vcpuid_vcat_04h(const struct acrn_vm *vm, struct vcpuid_entry *entry)
{
	uint32_t cache_type = entry->eax & 0x1FU; /* EAX bits 04:00 */
	uint32_t cache_level = (entry->eax >> 5U) & 0x7U; /* EAX bits 07:05 */
	uint16_t vcbm_len = 0U;

	if (cache_level == 2U) {
		vcbm_len = vcat_get_vcbm_len(vm, RDT_RESOURCE_L2);
	} else if (cache_level == 3U) {
		vcbm_len = vcat_get_vcbm_len(vm, RDT_RESOURCE_L3);
	}

	/*
	 * cache_type:
	 * 0 = Null - No more caches.
	 * 1 = Data Cache.
	 * 2 = Instruction Cache.
	 * 3 = Unified Cache.
	 * 4-31 = Reserved
	 *
	 * cache_level (starts at 1):
	 * 2 = L2
	 * 3 = L3
	 */
	if (((cache_type == 0x1U) || (cache_type == 0x3U)) && (vcbm_len != 0U)) {
		/*
		 * EBX Bits 11 - 00: L = System Coherency Line Size**.
		 * Bits 21 - 12: P = Physical Line partitions**.
		 * Bits 31 - 22: W = Ways of associativity**.
		 */
		entry->ebx &= ~0xFFC00000U;
		/* Report # of cache ways (CBM length) to guest VM */
		entry->ebx |= (vcbm_len - 1U) << 22U;
	}

	return 0;
}

/**
 * @brief RDT allocation enumeration sub-leaf (EAX = 10H, ECX = 0)
 * Expose CAT capabilities to guest VM
 *
 * @pre vm != NULL
 */
static int32_t set_vcpuid_vcat_10h_subleaf_0(struct acrn_vm *vm, bool l2, bool l3)
{
	struct vcpuid_entry entry;

	init_vcpuid_entry(CPUID_RDT_ALLOCATION, 0U, CPUID_CHECK_SUBLEAF, &entry);

	entry.ebx &= ~0xeU; /* Set the L3/L2/MBA bits (bits 1, 2, and 3) all to 0 (not supported) */

	if (l2) {
		/* Bit 02: Supports L2 Cache Allocation Technology if 1 */
		entry.ebx |= 0x4U;
	}

	if (l3) {
		/* Bit 01: Supports L3 Cache Allocation Technology if 1 */
		entry.ebx |= 0x2U;
	}

	return set_vcpuid_entry(vm, &entry);
}

/**
 * @brief L2/L3 enumeration sub-leaf
 *
 * @pre vm != NULL
 */
static int32_t set_vcpuid_vcat_10h_subleaf_res(struct acrn_vm *vm, uint32_t subleaf, uint16_t num_vclosids)
{
	struct vcpuid_entry entry;
	uint16_t vcbm_len;
	int res;

	if (subleaf == 1U) {
		res = RDT_RESOURCE_L3;
	} else {
		res = RDT_RESOURCE_L2;
	}
	vcbm_len = vcat_get_vcbm_len(vm, res);

	/* Set cache cbm_len */
	init_vcpuid_entry(CPUID_RDT_ALLOCATION, subleaf, CPUID_CHECK_SUBLEAF, &entry);

	if ((entry.eax != 0U) && (vcbm_len != 0U)) {
		/* Bits 4 - 00: Length of the capacity bit mask for the corresponding ResID using minus-one notation */
		entry.eax = (entry.eax & ~0x1F) | (vcbm_len - 1U);

		/* Bits 31 - 00: Bit-granular map of isolation/contention of allocation units
		 * Each set bit within the length of the CBM indicates the corresponding unit of the L2/L3 allocation
		 * may be used by other entities in the platform. Each cleared bit within the length of the CBM
		 * indicates the corresponding allocation unit can be configured to implement a priority-based
		 * allocation scheme chosen by an OS/VMM without interference with other hardware agents in the system.
		 */
		entry.ebx = (uint32_t)vcat_pcbm_to_vcbm(vm, entry.ebx, res);

		/* Do not support CDP for now */
		entry.ecx &= ~0x4U;

		/* Report max CLOS to guest VM
		 * Bits 15 - 00: Highest COS number supported for this ResID using minus-one notation
		 */
		entry.edx = (entry.edx & 0xFFFF0000U) |  (num_vclosids - 1U);
	}

	return set_vcpuid_entry(vm, &entry);
}

/**
 * @pre vm != NULL
 */
static int32_t set_vcpuid_vcat_10h(struct acrn_vm *vm)
{
	int32_t result;
	uint16_t num_vclosids = vcat_get_num_vclosids(vm);
	bool l2 = is_l2_vcat_configured(vm);
	bool l3 = is_l3_vcat_configured(vm);

	/* RDT allocation enumeration sub-leaf (EAX=10H, ECX=0) */
	result = set_vcpuid_vcat_10h_subleaf_0(vm, l2, l3);

	if ((result == 0) && l2) {
		/* L2 enumeration sub-leaf (EAX=10H, ECX=2) */
		result = set_vcpuid_vcat_10h_subleaf_res(vm, 2U, num_vclosids);
	}

	if ((result == 0) && l3) {
		/* L3 enumeration sub-leaf (EAX=10H, ECX=1) */
		result = set_vcpuid_vcat_10h_subleaf_res(vm, 1U, num_vclosids);
	}

	return result;
}
#endif

static void guest_cpuid_04h(__unused struct acrn_vm *vm, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	struct vcpuid_entry entry;

	cpuid_subleaf(CPUID_CACHE, *ecx, &entry.eax, &entry.ebx, &entry.ecx, &entry.edx);
	if (entry.eax != 0U) {
#ifdef CONFIG_VCAT_ENABLED
		if (is_vcat_configured(vm)) {
			/* set_vcpuid_vcat_04h will not change entry.eax */
			result = set_vcpuid_vcat_04h(vm, &entry);
		}
#endif
	}
	*eax = entry.eax;
	*ebx = entry.ebx;
	*ecx = entry.ecx;
	*edx = entry.edx;
}

static int32_t set_vcpuid_cache(struct acrn_vm *vm)
{
	int32_t result = 0;
	struct vcpuid_entry entry;
	uint32_t i;

	entry.leaf = CPUID_CACHE;
	entry.flags = CPUID_CHECK_SUBLEAF;
	for (i = 0U; ; i++) {
		entry.subleaf = i;
		entry.ecx = i;
		guest_cpuid_04h(vm, &entry.eax, &entry.ebx, &entry.ecx, &entry.edx);
		if (entry.eax == 0U) {
			break;
		}

		result = set_vcpuid_entry(vm, &entry);
		if (result != 0) {
			/* wants to break out of switch */
			break;
		}
	}
	return result;
}

static int32_t set_vcpuid_extfeat(struct acrn_vm *vm)
{
	uint64_t cr4_reserved_mask = get_cr4_reserved_bits();
	int32_t result = 0;
	struct vcpuid_entry entry;
	uint32_t i, sub_leaves;

	/* cpuid.07h.0h */
	cpuid_subleaf(CPUID_EXTEND_FEATURE, 0U, &entry.eax, &entry.ebx, &entry.ecx, &entry.edx);

	entry.ebx &= ~(CPUID_EBX_PQM | CPUID_EBX_PQE);
	if (is_vsgx_supported(vm->vm_id)) {
		entry.ebx |= CPUID_EBX_SGX;
	}

#ifdef CONFIG_VCAT_ENABLED
	if (is_vcat_configured(vm)) {
		/* Bit 15: Supports Intel Resource Director Technology (Intel RDT) Allocation capability if 1 */
		entry.ebx |= CPUID_EBX_PQE;
	}
#endif
	/* mask LA57 */
	entry.ecx &= ~CPUID_ECX_LA57;

	/* mask SGX and SGX_LC */
	entry.ebx &= ~CPUID_EBX_SGX;
	entry.ecx &= ~CPUID_ECX_SGX_LC;

	/* mask MPX */
	entry.ebx &= ~CPUID_EBX_MPX;

	/* mask Intel Processor Trace, since 14h is disabled */
	entry.ebx &= ~CPUID_EBX_PROC_TRC;

	/* mask CET shadow stack and indirect branch tracking */
	entry.ecx &= ~CPUID_ECX_CET_SS;
	entry.edx &= ~CPUID_EDX_CET_IBT;

	/* mask WAITPKG */
	entry.ecx &= ~CPUID_ECX_WAITPKG;

	if ((cr4_reserved_mask & CR4_FSGSBASE) != 0UL) {
		entry.ebx &= ~CPUID_EBX_FSGSBASE;
	}

	if ((cr4_reserved_mask & CR4_SMEP) != 0UL) {
		entry.ebx &= ~CPUID_EBX_SMEP;
	}

	if ((cr4_reserved_mask & CR4_SMAP) != 0UL) {
		entry.ebx &= ~CPUID_EBX_SMAP;
	}

	if ((cr4_reserved_mask & CR4_UMIP) != 0UL) {
		entry.ecx &= ~CPUID_ECX_UMIP;
	}

	if ((cr4_reserved_mask & CR4_PKE) != 0UL) {
		entry.ecx &= ~CPUID_ECX_PKE;
	}

	if ((cr4_reserved_mask & CR4_LA57) != 0UL) {
		entry.ecx &= ~CPUID_ECX_LA57;
	}

	if ((cr4_reserved_mask & CR4_PKS) != 0UL) {
		entry.ecx &= ~CPUID_ECX_PKS;
	}

	entry.leaf = CPUID_EXTEND_FEATURE;
	entry.subleaf = 0U;
	entry.flags = CPUID_CHECK_SUBLEAF;
	result = set_vcpuid_entry(vm, &entry);
	if (result == 0) {
		sub_leaves = entry.eax;
		for (i = 1U; i <= sub_leaves; i++) {
			cpuid_subleaf(CPUID_EXTEND_FEATURE, i, &entry.eax, &entry.ebx, &entry.ecx, &entry.edx);
			entry.subleaf = i;
			result = set_vcpuid_entry(vm, &entry);
			if (result != 0) {
				break;
			}
		}
	}
	return result;
}

static void guest_cpuid_06h(struct acrn_vm *vm, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	cpuid_subleaf(CPUID_THERMAL_POWER, *ecx, eax, ebx, ecx, edx);

	/* Always hide package level HWP controls and HWP interrupt*/
	*eax &= ~(CPUID_EAX_HWP_CTL | CPUID_EAX_HWP_PLR | CPUID_EAX_HWP_N);
	*eax &= ~(CPUID_EAX_HFI | CPUID_EAX_ITD);
	/* Since HFI is hidden, hide the edx too */
	*edx = 0U;
	if (!is_vhwp_configured(vm)) {
		*eax &= ~(CPUID_EAX_HWP | CPUID_EAX_HWP_AW | CPUID_EAX_HWP_EPP);
		*ecx &= ~CPUID_ECX_HCFC;
	}
}

static int32_t set_vcpuid_thermal_power(struct acrn_vm *vm)
{
	struct vcpuid_entry entry;

	entry.leaf = CPUID_THERMAL_POWER;
	entry.subleaf = 0;
	entry.ecx = 0;
	entry.flags = CPUID_CHECK_SUBLEAF;
	guest_cpuid_06h(vm, &entry.eax, &entry.ebx, &entry.ecx, &entry.edx);

	return set_vcpuid_entry(vm, &entry);
}

static int32_t set_vcpuid_extended_function(struct acrn_vm *vm)
{
	uint32_t i, limit;
	struct vcpuid_entry entry;
	int32_t result;

	init_vcpuid_entry(0x40000000U, 0U, 0U, &entry);
	result = set_vcpuid_entry(vm, &entry);
	if (result == 0) {
		init_vcpuid_entry(0x40000001U, 0U, 0U, &entry);
		/* EAX: Guest capability flags (e.g. whether it is a privilege VM) */
		if (is_service_vm(vm)) {
			entry.eax |= GUEST_CAPS_PRIVILEGE_VM;
		}
#ifdef CONFIG_HYPERV_ENABLED
		else {
			hyperv_init_vcpuid_entry(0x40000001U, 0U, 0U, &entry);
		}
#endif
		result = set_vcpuid_entry(vm, &entry);
	}

#ifdef CONFIG_HYPERV_ENABLED
	if (result == 0) {
		for (i = 0x40000002U; i <= 0x40000006U; i++) {
			hyperv_init_vcpuid_entry(i, 0U, 0U, &entry);
			result = set_vcpuid_entry(vm, &entry);
			if (result != 0) {
				break;
			}
		}
	}
#endif

	if (result == 0) {
		init_vcpuid_entry(0x40000010U, 0U, 0U, &entry);
		result = set_vcpuid_entry(vm, &entry);
	}

	if (result == 0) {
		init_vcpuid_entry(0x80000000U, 0U, 0U, &entry);
		result = set_vcpuid_entry(vm, &entry);
	}

	if (result == 0) {
		limit = entry.eax;
		vm->vcpuid_xlevel = limit;
		for (i = 0x80000002U; i <= limit; i++) {
			init_vcpuid_entry(i, 0U, 0U, &entry);
			result = set_vcpuid_entry(vm, &entry);
			if (result != 0) {
				break;
			}
		}
	}

	return result;
}

static inline bool is_percpu_related(uint32_t leaf)
{
	uint32_t i;
	bool ret = false;

	for (i = 0; i < pcpu_cpuids.leaf_nr; i++) {
		if (leaf == pcpu_cpuids.leaves[i]) {
			ret = true;
			break;
		}
	}
	return ret;
}

static inline void percpu_cpuid_init(void)
{
	/* 0x1U, 0xBU, 0xDU, 0x19U, 0x1FU, 0x80000001U */
	uint32_t percpu_leaves[] = {CPUID_FEATURES, CPUID_EXTEND_TOPOLOGY,
		CPUID_XSAVE_FEATURES, CPUID_KEY_LOCKER,
		CPUID_V2_EXTEND_TOPOLOGY, CPUID_EXTEND_FUNCTION_1};

	pcpu_cpuids.leaf_nr = sizeof(percpu_leaves)/sizeof(uint32_t);
	memcpy_s(pcpu_cpuids.leaves, sizeof(percpu_leaves),
		 percpu_leaves, sizeof(percpu_leaves));

	/* hybrid related percpu leaves*/
	if (pcpu_has_cap(X86_FEATURE_HYBRID)) {
		/* 0x2U, 0x4U, 0x6U, 0x14U, 0x16U, 0x18U, 0x1A, 0x1C, 0x80000006U */
		uint32_t hybrid_leaves[] = {CPUID_TLB, CPUID_CACHE,
			CPUID_THERMAL_POWER, CPUID_TRACE, CPUID_FREQ,
			CPUID_ADDR_TRANS, CPUID_MODEL_ID, CPUID_LAST_BRANCH_RECORD,
			CPUID_EXTEND_CACHE};
		memcpy_s(pcpu_cpuids.leaves + pcpu_cpuids.leaf_nr,
			 sizeof(hybrid_leaves), hybrid_leaves, sizeof(hybrid_leaves));
		pcpu_cpuids.leaf_nr += sizeof(hybrid_leaves)/sizeof(uint32_t);
	}
}

int32_t set_vcpuid_entries(struct acrn_vm *vm)
{
	int32_t result;
	struct vcpuid_entry entry;
	uint32_t limit;
	uint32_t i;
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();

	init_vcpuid_entry(0U, 0U, 0U, &entry);
	if (cpu_info->cpuid_level < 0x16U) {
		/* The cpuid with zero leaf returns the max level. Emulate that the 0x16U is supported */
		entry.eax = 0x16U;
	}
	result = set_vcpuid_entry(vm, &entry);
	if (result == 0) {
		percpu_cpuid_init();

		limit = entry.eax;
		vm->vcpuid_level = limit;

		for (i = 1U; i <= limit; i++) {
			if (is_percpu_related(i)) {
				continue;
			}

			switch (i) {
			/* 0x4U */
			case CPUID_CACHE:
				result = set_vcpuid_cache(vm);
				break;
			/* MONITOR/MWAIT */
			case 0x05U:
				break;
			/* 0x06U */
			case CPUID_THERMAL_POWER:
				result = set_vcpuid_thermal_power(vm);
				break;
			 /* 0x07U */
			case CPUID_EXTEND_FEATURE:
				result = set_vcpuid_extfeat(vm);
				break;
			/* 0x12U */
			case CPUID_SGX_CAP:
				result = set_vcpuid_sgx(vm);
				break;
			/* These features are disabled */
			/* PMU is not supported except for core partition VM, like RTVM */
			/* 0x0aU */
			case CPUID_ARCH_PERF_MON:
				if (is_pmu_pt_configured(vm)) {
					init_vcpuid_entry(i, 0U, 0U, &entry);
					result = set_vcpuid_entry(vm, &entry);
				}
				break;

			/* 0xFU, Intel RDT */
			case CPUID_RDT_MONITOR:
				break;
			/* 0x10U, Intel RDT */
			case CPUID_RDT_ALLOCATION:
#ifdef CONFIG_VCAT_ENABLED
				if (is_vcat_configured(vm)) {
					result = set_vcpuid_vcat_10h(vm);
				}
#endif
				break;

			/* 0x14U, Intel Processor Trace */
			case CPUID_TRACE:
			/* 0x1BU, PCONFIG */
			case CPUID_PCONFIG:
				break;
			default:
				init_vcpuid_entry(i, 0U, 0U, &entry);
				result = set_vcpuid_entry(vm, &entry);
				break;
			}

			/* WARNING: do nothing between break out of switch and before this check */
			if (result != 0) {
				/* break out of for */
				break;
			}
		}

		if (result == 0) {
			result = set_vcpuid_extended_function(vm);
		}
	}

	return result;
}

static void guest_cpuid_01h(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t apicid = vlapic_get_apicid(vcpu_vlapic(vcpu));
	uint64_t cr4_reserved_mask = get_cr4_reserved_bits();

	cpuid_subleaf(0x1U, 0x0U, eax, ebx, ecx, edx);
	/* Patching initial APIC ID */
	*ebx &= ~APIC_ID_MASK;
	*ebx |= (apicid <<  APIC_ID_SHIFT);

	if (vm_hide_mtrr(vcpu->vm)) {
		/* mask mtrr */
		*edx &= ~CPUID_EDX_MTRR;
	}

	/* mask Safer Mode Extension */
	*ecx &= ~CPUID_ECX_SMX;

	*ecx &= ~CPUID_ECX_EST;

	/* mask SDBG for silicon debug */
	*ecx &= ~CPUID_ECX_SDBG;

	/* mask VMX to guest OS */
	if (!is_nvmx_configured(vcpu->vm)) {
		*ecx &= ~CPUID_ECX_VMX;
	}

	/* set Hypervisor Present Bit */
	*ecx |= CPUID_ECX_HV;

	if ((cr4_reserved_mask & CR4_PCIDE) != 0UL) {
		*ecx &= ~CPUID_ECX_PCID;
	}

	/*
	 * Hide MONITOR/MWAIT.
	 */
	*ecx &= ~CPUID_ECX_MONITOR;

	*ecx &= ~CPUID_ECX_OSXSAVE;
	if ((*ecx & CPUID_ECX_XSAVE) != 0U) {
		uint64_t cr4;
		/*read guest CR4*/
		cr4 = vcpu_get_cr4(vcpu);
		if ((cr4 & CR4_OSXSAVE) != 0UL) {
			*ecx |= CPUID_ECX_OSXSAVE;
		}
	}

	if ((cr4_reserved_mask & CR4_VME) != 0UL) {
		*edx &= ~CPUID_EDX_VME;
	}

	if ((cr4_reserved_mask & CR4_DE) != 0UL) {
		*edx &= ~CPUID_EDX_DE;
	}

	if ((cr4_reserved_mask & CR4_PSE) != 0UL) {
		*edx &= ~CPUID_EDX_PSE;
	}

	if ((cr4_reserved_mask & CR4_PAE) != 0UL) {
		*edx &= ~CPUID_EDX_PAE;
	}

	if ((cr4_reserved_mask & CR4_PGE) != 0UL) {
		*edx &= ~CPUID_EDX_PGE;
	}

	if ((cr4_reserved_mask & CR4_OSFXSR) != 0UL) {
		*edx &= ~CPUID_EDX_FXSR;
	}

	/* DS/PEBS is not supported except for core partition VM, like RTVM */
	if (!is_pmu_pt_configured(vcpu->vm)) {
		/* mask Debug Store feature */
		*ecx &= ~(CPUID_ECX_DTES64 | CPUID_ECX_DS_CPL);

		/* mask PDCM: Perfmon and Debug Capability */
		*ecx &= ~CPUID_ECX_PDCM;

		/* mask Debug Store feature */
		*edx &= ~CPUID_EDX_DTES;
	}
}

static void guest_cpuid_0bh(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	/* Forward host cpu topology to the guest, guest will know the native platform information such as host cpu topology here */
	cpuid_subleaf(0x0BU, *ecx, eax, ebx, ecx, edx);

	/* Patching X2APIC */
	*edx = vlapic_get_apicid(vcpu_vlapic(vcpu));
}

static void guest_cpuid_0dh(__unused struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t subleaf = *ecx;

	if (!pcpu_has_cap(X86_FEATURE_OSXSAVE)) {
				*eax = 0U;
				*ebx = 0U;
				*ecx = 0U;
				*edx = 0U;
	} else {
		cpuid_subleaf(0x0dU, subleaf, eax, ebx, ecx, edx);
		switch (subleaf) {
		case 0U:
			/* SDM Vol.1 17-2, On processors that do not support Intel MPX,
			 * CPUID.(EAX=0DH,ECX=0):EAX[3] and
			 * CPUID.(EAX=0DH,ECX=0):EAX[4] will both be 0 */
			*eax &= ~(CPUID_EAX_XCR0_BNDREGS | CPUID_EAX_XCR0_BNDCSR);
			break;
		case 1U:
			*ecx &= ~(CPUID_ECX_CET_U_STATE | CPUID_ECX_CET_S_STATE);
			break;
		case 11U:
		case 12U:
			*eax = 0U;
			*ebx = 0U;
			*ecx = 0U;
			*edx = 0U;
			break;
		default:
			/* No emulation for other leaves */
			break;
		}
	}
}

static void guest_cpuid_19h(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	if (pcpu_has_cap(X86_FEATURE_KEYLOCKER)) {
		/* Host CR4.KL should be enabled at boot time */
		cpuid_subleaf(0x19U, 0U, eax, ebx, ecx, edx);
		/* Guest CR4.KL determines KL_AES_ENABLED */
		*ebx &= ~(vcpu->arch.cr4_kl_enabled ? 0U : CPUID_EBX_KL_AES_EN);
		/* Don't support nobackup and randomization parameter of LOADIWKEY */
		*ecx &= ~(CPUID_ECX_KL_NOBACKUP | CPUID_ECX_KL_RANDOM_KS);
	} else {
		*eax = 0U;
		*ebx = 0U;
		*ecx = 0U;
		*edx = 0U;
	}
}

static void guest_cpuid_1fh(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	cpuid_subleaf(0x1fU, *ecx, eax, ebx, ecx, edx);

	/* Patching X2APIC */
	*edx = vlapic_get_apicid(vcpu_vlapic(vcpu));
}

static void guest_cpuid_80000001h(const struct acrn_vcpu *vcpu,
	uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	const struct vcpuid_entry *entry_check = find_vcpuid_entry(vcpu, 0x80000000U, 0);
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);
	uint32_t leaf = 0x80000001U;

	if ((entry_check != NULL) && (entry_check->eax >= leaf)) {
		cpuid_subleaf(leaf, 0x0U, eax, ebx, ecx, edx);
		/* SDM Vol4 2.1, XD Bit Disable of MSR_IA32_MISC_ENABLE
		 * When set to 1, the Execute Disable Bit feature (XD Bit) is disabled and the XD Bit
		 * extended feature flag will be clear (CPUID.80000001H: EDX[20]=0)
		 */
		if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_XD_DISABLE) != 0UL) {
			*edx = *edx & ~CPUID_EDX_XD_BIT_AVIL;
		}
	} else {
		*eax = 0U;
		*ebx = 0U;
		*ecx = 0U;
		*edx = 0U;
	}
}

static void guest_limit_cpuid(const struct acrn_vcpu *vcpu, uint32_t leaf,
	uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);

	if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_LIMIT_CPUID) != 0UL) {
		/* limit the leaf number to 2 */
		if (leaf == 0U) {
			*eax = 2U;
		} else if (leaf > 2U) {
			*eax = 0U;
			*ebx = 0U;
			*ecx = 0U;
			*edx = 0U;
		} else {
			/* In this case, leaf is 1U, return the cpuid value get above */
		}
	}
}

void guest_cpuid(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t leaf = *eax;
	uint32_t subleaf = *ecx;

	/* vm related */
	if (!is_percpu_related(leaf)) {
		const struct vcpuid_entry *entry = find_vcpuid_entry(vcpu, leaf, subleaf);

		if (entry != NULL) {
			*eax = entry->eax;
			*ebx = entry->ebx;
			*ecx = entry->ecx;
			*edx = entry->edx;
		} else {
			*eax = 0U;
			*ebx = 0U;
			*ecx = 0U;
			*edx = 0U;
		}
	} else {
		/* percpu related */
		switch (leaf) {
		/* 0x01U */
		case CPUID_FEATURES:
			guest_cpuid_01h(vcpu, eax, ebx, ecx, edx);
			break;

		/* 0x04U for hybrid arch */
		case CPUID_CACHE:
			guest_cpuid_04h(vcpu->vm, eax, ebx, ecx, edx);
			break;

		/* 0x06U for hybrid arch */
		case CPUID_THERMAL_POWER:
			guest_cpuid_06h(vcpu->vm, eax, ebx, ecx, edx);
			break;

		/* 0x0BU */
		case CPUID_EXTEND_TOPOLOGY:
			guest_cpuid_0bh(vcpu, eax, ebx, ecx, edx);
			break;

		/* 0x0dU */
		case CPUID_XSAVE_FEATURES:
			guest_cpuid_0dh(vcpu, eax, ebx, ecx, edx);
			break;

		/* 0x14U for hybrid arch */
		case CPUID_TRACE:
			*eax = 0U;
			*ebx = 0U;
			*ecx = 0U;
			*edx = 0U;
			break;
		/* 0x19U */
		case CPUID_KEY_LOCKER:
			guest_cpuid_19h(vcpu, eax, ebx, ecx, edx);
			break;

		/* 0x1fU */
		case CPUID_V2_EXTEND_TOPOLOGY:
			guest_cpuid_1fh(vcpu, eax, ebx, ecx, edx);
			break;

		/* 0x80000001U */
		case CPUID_EXTEND_FUNCTION_1:
			guest_cpuid_80000001h(vcpu, eax, ebx, ecx, edx);
			break;

		default:
			/*
			 * In this switch statement, leaf 0x01/0x04/0x06/0x0b/0x0d/0x19/0x1f/0x80000001
			 * shall be handled specifically. All the other cases
			 * just return physical value.
			 */
			cpuid_subleaf(leaf, *ecx, eax, ebx, ecx, edx);
			break;
		}
	}

	guest_limit_cpuid(vcpu, leaf, eax, ebx, ecx, edx);
}
