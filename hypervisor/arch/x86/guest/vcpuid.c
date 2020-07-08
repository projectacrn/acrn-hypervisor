/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <vcpu.h>
#include <vm.h>
#include <cpuid.h>
#include <cpufeatures.h>
#include <vmx.h>
#include <sgx.h>
#include <logmsg.h>

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
		}
		else {
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
	case 0x07U:
		if (subleaf == 0U) {
			cpuid_subleaf(leaf, subleaf, &entry->eax, &entry->ebx, &entry->ecx, &entry->edx);

			entry->ebx &= ~(CPUID_EBX_PQM | CPUID_EBX_PQE);

			/* mask SGX and SGX_LC */
			entry->ebx &= ~CPUID_EBX_SGX;
			entry->ecx &= ~CPUID_ECX_SGX_LC;

			/* mask MPX */
			entry->ebx &= ~CPUID_EBX_MPX;

			/* mask Intel Processor Trace, since 14h is disabled */
			entry->ebx &= ~CPUID_EBX_PROC_TRC;

			/* mask CET shadow stack and indirect branch tracking */
			entry->ecx &= ~CPUID_ECX_CET_SS;
			entry->edx &= ~CPUID_EDX_CET_IBT;
		} else {
			entry->eax = 0U;
			entry->ebx = 0U;
			entry->ecx = 0U;
			entry->edx = 0U;
		}
		break;

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
		struct epc_map* maps;
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
		if (is_sos_vm(vm)) {
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

int32_t set_vcpuid_entries(struct acrn_vm *vm)
{
	int32_t result;
	struct vcpuid_entry entry;
	uint32_t limit;
	uint32_t i, j;
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();

	init_vcpuid_entry(0U, 0U, 0U, &entry);
	if (cpu_info->cpuid_level < 0x16U) {
		/* The cpuid with zero leaf returns the max level. Emulate that the 0x16U is supported */
		entry.eax = 0x16U;
	}
	result = set_vcpuid_entry(vm, &entry);
	if (result == 0) {
		limit = entry.eax;
		vm->vcpuid_level = limit;

		for (i = 1U; i <= limit; i++) {
			/* cpuid 1/0xb is percpu related */
			if ((i == 1U) || (i == 0xbU) || (i == 0xdU)) {
				continue;
			}

			switch (i) {
			case 0x04U:
				for (j = 0U; ; j++) {
					init_vcpuid_entry(i, j, CPUID_CHECK_SUBLEAF, &entry);
					if (entry.eax == 0U) {
						break;
					}
					result = set_vcpuid_entry(vm, &entry);
					if (result != 0) {
						/* wants to break out of switch */
						break;
					}
				}
				break;
			case 0x07U:
				init_vcpuid_entry(i, 0U, CPUID_CHECK_SUBLEAF, &entry);
				if (entry.eax != 0U) {
					pr_warn("vcpuid: only support subleaf 0 for cpu leaf 07h");
					entry.eax = 0U;
				}
				if (is_vsgx_supported(vm->vm_id)) {
					entry.ebx |= CPUID_EBX_SGX;
				}
				result = set_vcpuid_entry(vm, &entry);
				break;
			case 0x12U:
				result = set_vcpuid_sgx(vm);
				break;
			/* These features are disabled */
			/* PMU is not supported */
			case 0x0aU:
			/* Intel RDT */
			case 0x0fU:
			case 0x10U:
			/* Intel Processor Trace */
			case 0x14U:
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

static inline bool is_percpu_related(uint32_t leaf)
{
	return ((leaf == 0x1U) || (leaf == 0xbU) || (leaf == 0xdU) || (leaf == 0x80000001U));
}

static void guest_cpuid_01h(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t apicid = vlapic_get_apicid(vcpu_vlapic(vcpu));
	uint64_t guest_ia32_misc_enable = vcpu_get_guest_msr(vcpu, MSR_IA32_MISC_ENABLE);

	cpuid_subleaf(0x1U, 0x0U, eax, ebx, ecx, edx);
	/* Patching initial APIC ID */
	*ebx &= ~APIC_ID_MASK;
	*ebx |= (apicid <<  APIC_ID_SHIFT);

	if (vm_hide_mtrr(vcpu->vm)) {
		/* mask mtrr */
		*edx &= ~CPUID_EDX_MTRR;
	}

	/* mask Debug Store feature */
	*ecx &= ~(CPUID_ECX_DTES64 | CPUID_ECX_DS_CPL);

	/* mask Safer Mode Extension */
	*ecx &= ~CPUID_ECX_SMX;

	/* mask PDCM: Perfmon and Debug Capability */
	*ecx &= ~CPUID_ECX_PDCM;

	/* mask SDBG for silicon debug */
	*ecx &= ~CPUID_ECX_SDBG;

	/*mask vmx to guest os */
	*ecx &= ~CPUID_ECX_VMX;

	/* set Hypervisor Present Bit */
	*ecx |= CPUID_ECX_HV;

	/* if guest disabed monitor/mwait, clear cpuid.01h[3] */
	if ((guest_ia32_misc_enable & MSR_IA32_MISC_ENABLE_MONITOR_ENA) == 0UL) {
		*ecx &= ~CPUID_ECX_MONITOR;
	}

	*ecx &= ~CPUID_ECX_OSXSAVE;
	if ((*ecx & CPUID_ECX_XSAVE) != 0U) {
		uint64_t cr4;
		/*read guest CR4*/
		cr4 = exec_vmread(VMX_GUEST_CR4);
		if ((cr4 & CR4_OSXSAVE) != 0UL) {
			*ecx |= CPUID_ECX_OSXSAVE;
		}
	}

	/* mask Debug Store feature */
	*edx &= ~CPUID_EDX_DTES;
}

static void guest_cpuid_0bh(struct acrn_vcpu *vcpu, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	uint32_t leaf = 0x0bU;
	uint32_t subleaf = *ecx;

	/* Patching X2APIC */
	if (is_sos_vm(vcpu->vm)) {
		cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx);
	} else {
		*ecx = subleaf & 0xFFU;
		/* No HT emulation for UOS */
		switch (subleaf) {
		case 0U:
			*eax = 0U;
			*ebx = 1U;
			*ecx |= (1U << 8U);
		break;
		case 1U:
			if (vcpu->vm->hw.created_vcpus == 1U) {
				*eax = 0U;
			} else {
				*eax = (uint32_t)fls32(vcpu->vm->hw.created_vcpus - 1U) + 1U;
			}
			*ebx = vcpu->vm->hw.created_vcpus;
			*ecx |= (2U << 8U);
		break;
		default:
			*eax = 0U;
			*ebx = 0U;
			*ecx |= (0U << 8U);
		break;
		}
	}
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
		case 0x01U:
			guest_cpuid_01h(vcpu, eax, ebx, ecx, edx);
			break;

		case 0x0bU:
			guest_cpuid_0bh(vcpu, eax, ebx, ecx, edx);
			break;

		case 0x0dU:
			guest_cpuid_0dh(vcpu, eax, ebx, ecx, edx);
			break;

		case 0x80000001U:
			guest_cpuid_80000001h(vcpu, eax, ebx, ecx, edx);
			break;

		default:
			/*
			 * In this switch statement, leaf shall either be 0x01U or 0x0bU
			 * or 0x0dU or 0x80000001U. All the other cases have been handled properly
			 * before this switch statement.
			 * Gracefully return if prior case clauses have not been met.
			 */
			break;
		}
	}

	guest_limit_cpuid(vcpu, leaf, eax, ebx, ecx, edx);
}
