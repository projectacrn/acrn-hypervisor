/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <cpu.h>

void emulate_cpuid(struct vcpu *vcpu, uint32_t src_op, uint32_t *eax_ptr,
	uint32_t *ebx_ptr, uint32_t *ecx_ptr, uint32_t *edx_ptr)
{
	uint32_t apicid = vlapic_get_id(vcpu->arch_vcpu.vlapic);
	static const char sig[12] = "ACRNACRNACRN";
	const uint32_t *sigptr = (const uint32_t *)sig;
	uint32_t count = *ecx_ptr;

	if ((src_op != 0x40000000) && (src_op != 0x40000010))
		cpuid_count(src_op, count, eax_ptr, ebx_ptr, ecx_ptr, edx_ptr);

	switch (src_op) {
		/* Virtualize cpuid 0x01 */
	case 0x01:
		/* Patching initial APIC ID */
		*ebx_ptr &= ~APIC_ID_MASK;
		*ebx_ptr |= (apicid & APIC_ID_MASK);

		/* mask mtrr */
		*edx_ptr &= ~CPUID_EDX_MTRR;

		/* Patching X2APIC, X2APIC mode is disabled by default. */
		if (x2apic_enabled)
			*ecx_ptr |= CPUID_ECX_x2APIC;
		else
			*ecx_ptr &= ~CPUID_ECX_x2APIC;

		/* mask pcid */
		*ecx_ptr &= ~CPUID_ECX_PCID;

		/*mask vmx to guest os */
		*ecx_ptr &= ~CPUID_ECX_VMX;

		break;

		/* Virtualize cpuid 0x07 */
	case 0x07:
		/* mask invpcid */
		*ebx_ptr &= ~CPUID_EBX_INVPCID;

		break;

	case 0x0a:
		/* not support pmu */
		*eax_ptr &= ~0xff;
		break;

		/* Virtualize cpuid 0x0b */
	case 0x0b:
		/* Patching X2APIC */
		if (!x2apic_enabled) {
			*eax_ptr = 0;
			*ebx_ptr = 0;
			*ecx_ptr = 0;
			*edx_ptr = 0;
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
	case 0x40000000:
		*eax_ptr = 0x40000010;
		*ebx_ptr = sigptr[0];
		*ecx_ptr = sigptr[1];
		*edx_ptr = sigptr[2];
		break;

		/*
		* Leaf 0x40000010 - Timing Information.
		* This leaf returns the current TSC frequency and
		* current Bus frequency in kHz.
		*
		* EAX: (Virtual) TSC frequency in kHz.
		*      TSC frequency is calculated from PIT in ACRN
		* EBX: (Virtual) Bus (local apic timer) frequency in kHz.
		*      Bus (local apic timer) frequency is hardcoded as
		*      (128 * 1024 * 1024) in ACRN
		* ECX, EDX: RESERVED (reserved fields are set to zero).
		*/
	case 0x40000010:
		*eax_ptr = (uint32_t)(tsc_clock_freq / 1000);
		*ebx_ptr = (128 * 1024 * 1024) / 1000;
		*ecx_ptr = 0;
		*edx_ptr = 0;
		break;

	default:
		break;
	}
}

static DEFINE_CPU_DATA(struct cpuid_cache_entry[CPUID_EXTEND_FEATURE_CACHE_MAX],
		cpuid_cache);

static inline struct cpuid_cache_entry *find_cpuid_cache_entry(uint32_t op,
	uint32_t count)
{
	int pcpu_id = get_cpu_id();
	enum cpuid_cache_idx idx = CPUID_EXTEND_FEATURE_CACHE_MAX;

	if ((count != 0))
		return NULL;

	switch (op) {
	case CPUID_VENDORSTRING:
		idx = CPUID_VENDORSTRING_CACHE_IDX;
		break;

	case CPUID_FEATURES:
		idx = CPUID_FEATURES_CACHE_IDX;
		break;

	case CPUID_EXTEND_FEATURE:
		idx = CPUID_EXTEND_FEATURE_CACHE_IDX;
		break;

	default:
		break;
	}

	if (idx == CPUID_EXTEND_FEATURE_CACHE_MAX)
		return NULL;

	return &per_cpu(cpuid_cache, pcpu_id)[idx];
}

inline void cpuid_count(uint32_t op, uint32_t count,
	uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
	struct cpuid_cache_entry *entry;

	entry = find_cpuid_cache_entry(op, count);

	if (entry == NULL) {
		native_cpuid_count(op, count, a, b, c, d);
	} else if (entry->inited) {
		*a = entry->a;
		*b = entry->b;
		*c = entry->c;
		*d = entry->d;
	} else {
		native_cpuid_count(op, count, a, b, c, d);

		entry->a = *a;
		entry->b = *b;
		entry->c = *c;
		entry->d = *d;

		entry->inited = 1;
	}
}

