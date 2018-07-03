/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static inline struct vcpuid_entry *find_vcpuid_entry(struct vcpu *vcpu,
					uint32_t leaf, uint32_t subleaf)
{
	int i = 0, nr, half;
	struct vcpuid_entry *entry = NULL;
	struct vm *vm = vcpu->vm;

	nr = vm->vcpuid_entry_nr;
	half = nr / 2;
	if (vm->vcpuid_entries[half].leaf < leaf)
		i = half;

	for (; i < nr; i++) {
		struct vcpuid_entry *tmp = &vm->vcpuid_entries[i];

		if (tmp->leaf < leaf)
			continue;
		if (tmp->leaf == leaf) {
			if ((tmp->flags & CPUID_CHECK_SUBLEAF) != 0U &&
				(tmp->subleaf != subleaf))
				continue;
			entry = tmp;
			break;
		} else if (tmp->leaf > leaf)
			break;
	}

	if (entry == NULL) {
		uint32_t limit;

		if ((leaf & 0x80000000) != 0U)
			limit = vm->vcpuid_xlevel;
		else
			limit = vm->vcpuid_level;

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

static inline int set_vcpuid_entry(struct vm *vm,
				struct vcpuid_entry *entry)
{
	struct vcpuid_entry *tmp;
	size_t entry_size = sizeof(struct vcpuid_entry);

	if (vm->vcpuid_entry_nr == MAX_VM_VCPUID_ENTRIES) {
		pr_err("%s, vcpuid entry over MAX_VM_VCPUID_ENTRIES(%u)\n",
				__func__, MAX_VM_VCPUID_ENTRIES);
		return -ENOMEM;
	}

	tmp = &vm->vcpuid_entries[vm->vcpuid_entry_nr++];
	memcpy_s(tmp, entry_size, entry, entry_size);
	return 0;
}

/**
 * initialization of virtual CPUID leaf
 */
static void init_vcpuid_entry(__unused struct vm *vm,
			uint32_t leaf, uint32_t subleaf,
			uint32_t flags, struct vcpuid_entry *entry)
{
	entry->leaf = leaf;
	entry->subleaf = subleaf;
	entry->flags = flags;

	switch (leaf) {
	case 0x07:
		if (subleaf == 0U) {
			cpuid(leaf,
				&entry->eax, &entry->ebx,
				&entry->ecx, &entry->edx);
			/* mask invpcid */
			entry->ebx &= ~CPUID_EBX_INVPCID;
		} else {
			entry->eax = 0;
			entry->ebx = 0;
			entry->ecx = 0;
			entry->edx = 0;
		}
		break;

	case 0x0a:
		/* not support pmu */
		entry->eax = 0;
		entry->ebx = 0;
		entry->ecx = 0;
		entry->edx = 0;
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
	case 0x40000000:
	{
		static const char sig[12] = "ACRNACRNACRN";
		const uint32_t *sigptr = (const uint32_t *)sig;

		entry->eax = 0x40000010;
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
	case 0x40000010:
		entry->eax = tsc_khz;
		entry->ebx = 0;
		entry->ecx = 0;
		entry->edx = 0;
		break;

	default:
		cpuid_subleaf(leaf, subleaf,
				&entry->eax, &entry->ebx,
				&entry->ecx, &entry->edx);
		break;
	}
}

int set_vcpuid_entries(struct vm *vm)
{
	int result;
	struct vcpuid_entry entry;
	uint32_t limit;
	uint32_t i, j;

	init_vcpuid_entry(vm, 0, 0, 0, &entry);
	result = set_vcpuid_entry(vm, &entry);
	if (result != 0)
		return result;
	vm->vcpuid_level = limit = entry.eax;

	for (i = 1; i <= limit; i++) {
		/* cpuid 1/0xb is percpu related */
		if (i == 1 || i == 0xb)
			continue;

		switch (i) {
		case 0x02:
		{
			uint32_t times;

			init_vcpuid_entry(vm, i, 0,
				CPUID_CHECK_SUBLEAF, &entry);
			result = set_vcpuid_entry(vm, &entry);
			if (result != 0)
				return result;

			times = entry.eax & 0xffUL;
			for (j = 1; j < times; j++) {
				init_vcpuid_entry(vm, i, j,
					CPUID_CHECK_SUBLEAF, &entry);
				result = set_vcpuid_entry(vm, &entry);
				if (result != 0)
					return result;
			}
			break;
		}

		case 0x04:
		case 0x0d:
			for (j = 0; ; j++) {
				if (i == 0x0d && j == 64)
					break;

				init_vcpuid_entry(vm, i, j,
					CPUID_CHECK_SUBLEAF, &entry);
				if (i == 0x04 && entry.eax == 0)
					break;
				if (i == 0x0d && entry.eax == 0)
					continue;
				result = set_vcpuid_entry(vm, &entry);
				if (result != 0)
					return result;
			}
			break;

		default:
			init_vcpuid_entry(vm, i, 0, 0, &entry);
			result = set_vcpuid_entry(vm, &entry);
			if (result != 0)
				return result;
			break;
		}
	}

	init_vcpuid_entry(vm, 0x40000000, 0, 0, &entry);
	result = set_vcpuid_entry(vm, &entry);
	if (result != 0)
		return result;

	init_vcpuid_entry(vm, 0x40000010, 0, 0, &entry);
	result = set_vcpuid_entry(vm, &entry);
	if (result != 0)
		return result;

	init_vcpuid_entry(vm, 0x80000000, 0, 0, &entry);
	result = set_vcpuid_entry(vm, &entry);
	if (result != 0)
		return result;

	vm->vcpuid_xlevel = limit = entry.eax;
	for (i = 0x80000001; i <= limit; i++) {
		init_vcpuid_entry(vm, i, 0, 0, &entry);
		result = set_vcpuid_entry(vm, &entry);
		if (result != 0)
			return result;
	}

	return 0;
}

void guest_cpuid(struct vcpu *vcpu,
		uint32_t *eax, uint32_t *ebx,
		uint32_t *ecx, uint32_t *edx)
{
	uint32_t leaf = *eax;
	uint32_t subleaf = *ecx;

	/* vm related */
	if (leaf != 0x1 && leaf != 0xb && leaf != 0xd) {
		struct vcpuid_entry *entry =
			find_vcpuid_entry(vcpu, leaf, subleaf);

		if (entry != NULL) {
			*eax = entry->eax;
			*ebx = entry->ebx;
			*ecx = entry->ecx;
			*edx = entry->edx;
		} else {
			*eax = 0;
			*ebx = 0;
			*ecx = 0;
			*edx = 0;
		}

		return;
	}

	/* percpu related */
	switch (leaf) {
	case 0x01:
	{
		cpuid(leaf, eax, ebx, ecx, edx);
		uint32_t apicid = vlapic_get_id(vcpu->arch_vcpu.vlapic);
		/* Patching initial APIC ID */
		*ebx &= ~APIC_ID_MASK;
		*ebx |= (apicid & APIC_ID_MASK);

#ifndef CONFIG_MTRR_ENABLED
		/* mask mtrr */
		*edx &= ~CPUID_EDX_MTRR;
#endif

		/* Patching X2APIC, X2APIC mode is disabled by default. */
		if (x2apic_enabled)
			*ecx |= CPUID_ECX_x2APIC;
		else
			*ecx &= ~CPUID_ECX_x2APIC;

		/* mask pcid */
		*ecx &= ~CPUID_ECX_PCID;

		/*mask vmx to guest os */
		*ecx &= ~CPUID_ECX_VMX;

		/*no xsave support for guest if it is not enabled on host*/
		if ((*ecx & CPUID_ECX_OSXSAVE) == 0U)
			*ecx &= ~CPUID_ECX_XSAVE;

		*ecx &= ~CPUID_ECX_OSXSAVE;
		if ((*ecx & CPUID_ECX_XSAVE) != 0U) {
			uint64_t cr4;
			/*read guest CR4*/
			cr4 = exec_vmread(VMX_GUEST_CR4);
			if ((cr4 & CR4_OSXSAVE) != 0U)
				*ecx |= CPUID_ECX_OSXSAVE;
		}
		break;
	}

	case 0x0b:
		/* Patching X2APIC */
		if (!x2apic_enabled) {
			*eax = 0;
			*ebx = 0;
			*ecx = 0;
			*edx = 0;
		} else
			cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx);
		break;

	case 0x0d:
		if (!cpu_has_cap(X86_FEATURE_OSXSAVE)) {
			*eax = 0;
			*ebx = 0;
			*ecx = 0;
			*edx = 0;
		} else
			cpuid_subleaf(leaf, subleaf, eax, ebx, ecx, edx);
		break;

	default:
		break;
	}
}
