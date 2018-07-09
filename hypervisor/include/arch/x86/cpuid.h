/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
* cpuid.h
*
*  Created on: Jan 4, 2018
*      Author: don
*/

#ifndef CPUID_H_
#define CPUID_H_

/* CPUID bit definitions */
#define CPUID_ECX_SSE3          (1U<<0)
#define CPUID_ECX_PCLMUL        (1U<<1)
#define CPUID_ECX_DTES64        (1U<<2)
#define CPUID_ECX_MONITOR       (1U<<3)
#define CPUID_ECX_DS_CPL        (1U<<4)
#define CPUID_ECX_VMX           (1U<<5)
#define CPUID_ECX_SMX           (1U<<6)
#define CPUID_ECX_EST           (1U<<7)
#define CPUID_ECX_TM2           (1U<<8)
#define CPUID_ECX_SSSE3         (1U<<9)
#define CPUID_ECX_CID           (1U<<10)
#define CPUID_ECX_FMA           (1U<<12)
#define CPUID_ECX_CX16          (1U<<13)
#define CPUID_ECX_ETPRD         (1U<<14)
#define CPUID_ECX_PDCM          (1U<<15)
#define CPUID_ECX_DCA           (1U<<18)
#define CPUID_ECX_SSE4_1        (1U<<19)
#define CPUID_ECX_SSE4_2        (1U<<20)
#define CPUID_ECX_x2APIC        (1U<<21)
#define CPUID_ECX_MOVBE         (1U<<22)
#define CPUID_ECX_POPCNT        (1U<<23)
#define CPUID_ECX_AES           (1U<<25)
#define CPUID_ECX_XSAVE         (1U<<26)
#define CPUID_ECX_OSXSAVE       (1U<<27)
#define CPUID_ECX_AVX           (1U<<28)
#define CPUID_EDX_FPU           (1U<<0)
#define CPUID_EDX_VME           (1U<<1)
#define CPUID_EDX_DE            (1U<<2)
#define CPUID_EDX_PSE           (1U<<3)
#define CPUID_EDX_TSC           (1U<<4)
#define CPUID_EDX_MSR           (1U<<5)
#define CPUID_EDX_PAE           (1U<<6)
#define CPUID_EDX_MCE           (1U<<7)
#define CPUID_EDX_CX8           (1U<<8)
#define CPUID_EDX_APIC          (1U<<9)
#define CPUID_EDX_SEP           (1U<<11)
#define CPUID_EDX_MTRR          (1U<<12)
#define CPUID_EDX_PGE           (1U<<13)
#define CPUID_EDX_MCA           (1U<<14)
#define CPUID_EDX_CMOV          (1U<<15)
#define CPUID_EDX_PAT           (1U<<16)
#define CPUID_EDX_PSE36         (1U<<17)
#define CPUID_EDX_PSN           (1U<<18)
#define CPUID_EDX_CLF           (1U<<19)
#define CPUID_EDX_DTES          (1U<<21)
#define CPUID_EDX_ACPI          (1U<<22)
#define CPUID_EDX_MMX           (1U<<23)
#define CPUID_EDX_FXSR          (1U<<24)
#define CPUID_EDX_SSE           (1U<<25)
#define CPUID_EDX_SSE2          (1U<<26)
#define CPUID_EDX_SS            (1U<<27)
#define CPUID_EDX_HTT           (1U<<28)
#define CPUID_EDX_TM1           (1U<<29)
#define CPUID_EDX_IA64          (1U<<30)
#define CPUID_EDX_PBE           (1U<<31)
/* CPUID.07H:EBX.TSC_ADJUST*/
#define CPUID_EBX_TSC_ADJ       (1U<<1)
/* CPUID.07H:EDX.IBRS_IBPB*/
#define CPUID_EDX_IBRS_IBPB     (1U<<26)
/* CPUID.07H:EDX.STIBP*/
#define CPUID_EDX_STIBP         (1U<<27)
/* CPUID.80000001H:EDX.Page1GB*/
#define CPUID_EDX_PAGE1GB       (1U<<26)
/* CPUID.07H:EBX.INVPCID*/
#define CPUID_EBX_INVPCID       (1U<<10)
/* CPUID.01H:ECX.PCID*/
#define CPUID_ECX_PCID          (1U<<17)

/* CPUID source operands */
#define CPUID_VENDORSTRING      0
#define CPUID_FEATURES          1
#define CPUID_TLB               2
#define CPUID_SERIALNUM         3
#define CPUID_EXTEND_FEATURE    7
#define CPUID_MAX_EXTENDED_FUNCTION  0x80000000U
#define CPUID_EXTEND_FUNCTION_1      0x80000001U
#define CPUID_EXTEND_FUNCTION_2      0x80000002U
#define CPUID_EXTEND_FUNCTION_3      0x80000003U
#define CPUID_EXTEND_FUNCTION_4      0x80000004U
#define CPUID_EXTEND_ADDRESS_SIZE    0x80000008U


static inline void __cpuid(uint32_t *eax, uint32_t *ebx,
				uint32_t *ecx, uint32_t *edx)
{
	/* Execute CPUID instruction and save results */
	asm volatile("cpuid":"=a"(*eax), "=b"(*ebx),
			"=c"(*ecx), "=d"(*edx)
			: "0" (*eax), "2" (*ecx)
			: "memory");
}

static inline void cpuid(uint32_t leaf,
			uint32_t *eax, uint32_t *ebx,
			uint32_t *ecx, uint32_t *edx)
{
	*eax = leaf;
	*ecx = 0U;

	__cpuid(eax, ebx, ecx, edx);
}

static inline void cpuid_subleaf(uint32_t leaf, uint32_t subleaf,
				uint32_t *eax, uint32_t *ebx,
				uint32_t *ecx, uint32_t *edx)
{
	*eax = leaf;
	*ecx = subleaf;

	__cpuid(eax, ebx, ecx, edx);
}

int set_vcpuid_entries(struct vm *vm);
void guest_cpuid(struct vcpu *vcpu,
			uint32_t *eax, uint32_t *ebx,
			uint32_t *ecx, uint32_t *edx);

#endif /* CPUID_H_ */
