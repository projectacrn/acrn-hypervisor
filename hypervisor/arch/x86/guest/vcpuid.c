/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static inline struct vcpuid_entry *find_vcpuid_entry(const struct acrn_vcpu *vcpu,
					uint32_t leaf_arg, uint32_t subleaf)
{
	uint32_t i = 0U, nr, half;
	struct vcpuid_entry *entry = NULL;
	struct acrn_vm *vm = vcpu->vm;
	uint32_t leaf = leaf_arg;

	nr = vm->vcpuid_entry_nr;
	half = nr >> 1U;
	if (vm->vcpuid_entries[half].leaf < leaf) {
		i = half;
	}

	for (; i < nr; i++) {
		struct vcpuid_entry *tmp = &vm->vcpuid_entries[i];

		if (tmp->leaf < leaf) {
			continue;
		} else if (tmp->leaf == leaf) {
			if (((tmp->flags & CPUID_CHECK_SUBLEAF) != 0U) &&
				(tmp->subleaf != subleaf)) {
				continue;
			}
			entry = tmp;
			break;
		} else {
			/* tmp->leaf > leaf */
			break;
		}
	}

	if (entry == NULL) {
		uint32_t limit;

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
			return find_vcpuid_entry(vcpu, leaf, subleaf);
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
		pr_err("%s, vcpuid entry over MAX_VM_VCPUID_ENTRIES(%u)\n",
				__func__, MAX_VM_VCPUID_ENTRIES);
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
	entry->leaf = leaf;
	entry->subleaf = subleaf;
	entry->flags = flags;

	switch (leaf) {
	case 0x07U:
		if (subleaf == 0U) {
			cpuid(leaf,
				&entry->eax, &entry->ebx,
				&entry->ecx, &entry->edx);
			/* mask invpcid */
			entry->ebx &= ~(CPUID_EBX_INVPCID |
					CPUID_EBX_PQM |
					CPUID_EBX_PQE);

			/* mask SGX and SGX_LC */
			entry->ebx &= ~CPUID_EBX_SGX;
			entry->ecx &= ~CPUID_ECX_SGX_LC;
		} else {
			entry->eax = 0U;
			entry->ebx = 0U;
			entry->ecx = 0U;
			entry->edx = 0U;
		}
		break;

	case 0x16U:
		if (boot_cpu_data.cpuid_level >= 0x16U) {
			/* call the cpuid when 0x16 is supported */
			cpuid_subleaf(leaf, subleaf,
				&entry->eax, &entry->ebx,
				&entry->ecx, &entry->edx);
		} else {
			/* Use the tsc to derive the emulated 0x16U cpuid. */
			entry->eax = (uint32_t) (tsc_khz / 1000U);
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
	 * Leaf 0x40000010 - Timing Information.
	 * This leaf returns the current TSC frequency and
	 * current Bus frequency in kHz.
	 *
	 * EAX: (Virtual) TSC frequency in kHz.
	 *      TSC frequency is calculated from PIT in ACRN
	 * EBX, ECX, EDX: RESERVED (reserved fields are set to zero).
	 */
	case 0x40000010U:
		entry->eax = tsc_khz;
		entry->ebx = 0U;
		entry->ecx = 0U;
		entry->edx = 0U;
		break;

	default:
		cpuid_subleaf(leaf, subleaf,
				&entry->eax, &entry->ebx,
				&entry->ecx, &entry->edx);
		break;
	}
}

int32_t set_vcpuid_entries(struct acrn_vm *vm)
{
	int32_t result;
	struct vcpuid_entry entry;
	uint32_t limit;
	uint32_t i, j;

	init_vcpuid_entry(0U, 0U, 0U, &entry);
	if (boot_cpu_data.cpuid_level < 0x16U) {
		/* The cpuid with zero leaf returns the max level.
		 * Emulate that the 0x16U is supported */
		entry.eax = 0x16U;
	}
	result = set_vcpuid_entry(vm, &entry);
	if (result == 0) {
		limit = entry.eax;
		vm->vcpuid_level = limit;

		for (i = 1U; i <= limit; i++) {
			/* cpuid 1/0xb is percpu related */
			if ((i == 1U) || (i == 0xbU)) {
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

			case 0x0dU:
				for (j = 0U; j < 64U; j++) {
					init_vcpuid_entry(i, j, CPUID_CHECK_SUBLEAF, &entry);
					if (entry.eax == 0U) {
						continue;
					}
					result = set_vcpuid_entry(vm, &entry);
					if (result != 0) {
						/* wants to break out of switch */
						break;
					}
				}
				break;

			/* These features are disabled */
#ifdef PROFILING_ON
			/* PMU is not supported */
			case 0x0aU:
#endif /* PROFILING_ON */
			/* Intel RDT */
			case 0x0fU:
			case 0x10U:
			/* Intel SGX */
			case 0x12U:
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
			init_vcpuid_entry(0x40000000U, 0U, 0U, &entry);
			result = set_vcpuid_entry(vm, &entry);
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
				for (i = 0x80000001U; i <= limit; i++) {
					init_vcpuid_entry(i, 0U, 0U, &entry);
					result = set_vcpuid_entry(vm, &entry);
					if (result != 0) {
						break;
					}
				}
			}
		}
	}

	return result;
}

void guest_cpuid(struct acrn_vcpu *vcpu,
		uint32_t *eax, uint32_t *ebx,
		uint32_t *ecx, uint32_t *edx)
{
	uint32_t leaf = *eax;
	uint32_t subleaf = *ecx;

	/* vm related */
	if ((leaf != 0x1U) && (leaf != 0xbU) && (leaf != 0xdU)) {
		struct vcpuid_entry *entry =
			find_vcpuid_entry(vcpu, leaf, subleaf);

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
		{
			cpuid(leaf, eax, ebx, ecx, edx);
			uint32_t apicid = vlapic_get_apicid(vcpu_vlapic(vcpu));
			/* Patching initial APIC ID */
			*ebx &= ~APIC_ID_MASK;
			*ebx |= (apicid <<  APIC_ID_SHIFT);

#ifndef CONFIG_MTRR_ENABLED
			/* mask mtrr */
			*edx &= ~CPUID_EDX_MTRR;
#endif

			/* mask pcid */
			*ecx &= ~CPUID_ECX_PCID;

			/*mask vmx to guest os */
			*ecx &= ~CPUID_ECX_VMX;

			/*no xsave support for guest if it is not enabled on host*/
			if ((*ecx & CPUID_ECX_OSXSAVE) == 0U) {
				*ecx &= ~CPUID_ECX_XSAVE;
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
			break;
		}

		case 0x0bU:
			/* Patching X2APIC */
#ifdef CONFIG_PARTITION_MODE
			cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx);
#else
			if (is_vm0(vcpu->vm)) {
				cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx);
			} else {
				*ecx = subleaf & 0xFFU;
				*edx = vlapic_get_apicid(vcpu_vlapic(vcpu));
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
#endif
			break;

		case 0x0dU:
			if (!cpu_has_cap(X86_FEATURE_OSXSAVE)) {
				*eax = 0U;
				*ebx = 0U;
				*ecx = 0U;
				*edx = 0U;
			} else {
				cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx);
			}
			break;

		default:
			/*
			 * In this switch statement, leaf shall either be 0x01U or 0x0bU
			 * or 0x0dU. All the other cases have been handled properly
			 * before this switch statement.
			 * Gracefully return if prior case clauses have not been met.
			 */
			break;
		}
	}
}
