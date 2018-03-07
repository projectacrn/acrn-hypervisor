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

/*
* cpuid.h
*
*  Created on: Jan 4, 2018
*      Author: don
*/

#ifndef CPUID_H_
#define CPUID_H_

/* CPUID bit definitions */
#define CPUID_ECX_SSE3          (1<<0)
#define CPUID_ECX_PCLMUL        (1<<1)
#define CPUID_ECX_DTES64        (1<<2)
#define CPUID_ECX_MONITOR       (1<<3)
#define CPUID_ECX_DS_CPL        (1<<4)
#define CPUID_ECX_VMX           (1<<5)
#define CPUID_ECX_SMX           (1<<6)
#define CPUID_ECX_EST           (1<<7)
#define CPUID_ECX_TM2           (1<<8)
#define CPUID_ECX_SSSE3         (1<<9)
#define CPUID_ECX_CID           (1<<10)
#define CPUID_ECX_FMA           (1<<12)
#define CPUID_ECX_CX16          (1<<13)
#define CPUID_ECX_ETPRD         (1<<14)
#define CPUID_ECX_PDCM          (1<<15)
#define CPUID_ECX_DCA           (1<<18)
#define CPUID_ECX_SSE4_1        (1<<19)
#define CPUID_ECX_SSE4_2        (1<<20)
#define CPUID_ECX_x2APIC        (1<<21)
#define CPUID_ECX_MOVBE         (1<<22)
#define CPUID_ECX_POPCNT        (1<<23)
#define CPUID_ECX_AES           (1<<25)
#define CPUID_ECX_XSAVE         (1<<26)
#define CPUID_ECX_OSXSAVE       (1<<27)
#define CPUID_ECX_AVX           (1<<28)
#define CPUID_EDX_FPU           (1<<0)
#define CPUID_EDX_VME           (1<<1)
#define CPUID_EDX_DE            (1<<2)
#define CPUID_EDX_PSE           (1<<3)
#define CPUID_EDX_TSC           (1<<4)
#define CPUID_EDX_MSR           (1<<5)
#define CPUID_EDX_PAE           (1<<6)
#define CPUID_EDX_MCE           (1<<7)
#define CPUID_EDX_CX8           (1<<8)
#define CPUID_EDX_APIC          (1<<9)
#define CPUID_EDX_SEP           (1<<11)
#define CPUID_EDX_MTRR          (1<<12)
#define CPUID_EDX_PGE           (1<<13)
#define CPUID_EDX_MCA           (1<<14)
#define CPUID_EDX_CMOV          (1<<15)
#define CPUID_EDX_PAT           (1<<16)
#define CPUID_EDX_PSE36         (1<<17)
#define CPUID_EDX_PSN           (1<<18)
#define CPUID_EDX_CLF           (1<<19)
#define CPUID_EDX_DTES          (1<<21)
#define CPUID_EDX_ACPI          (1<<22)
#define CPUID_EDX_MMX           (1<<23)
#define CPUID_EDX_FXSR          (1<<24)
#define CPUID_EDX_SSE           (1<<25)
#define CPUID_EDX_SSE2          (1<<26)
#define CPUID_EDX_SS            (1<<27)
#define CPUID_EDX_HTT           (1<<28)
#define CPUID_EDX_TM1           (1<<29)
#define CPUID_EDX_IA64          (1<<30)
#define CPUID_EDX_PBE           (1<<31)
/* CPUID.07H:EBX.TSC_ADJUST*/
#define CPUID_EBX_TSC_ADJ       (1<<1)
/* CPUID.07H:EDX.IBRS_IBPB*/
#define CPUID_EDX_IBRS_IBPB     (1<<26)
/* CPUID.07H:EDX.STIBP*/
#define CPUID_EDX_STIBP         (1<<27)
/* CPUID.80000001H:EDX.Page1GB*/
#define CPUID_EDX_PAGE1GB       (1<<26)
/* CPUID.07H:EBX.INVPCID*/
#define CPUID_EBX_INVPCID       (1<<10)
/* CPUID.01H:ECX.PCID*/
#define CPUID_ECX_PCID          (1<<17)

/* CPUID source operands */
#define CPUID_VENDORSTRING      0
#define CPUID_FEATURES          1
#define CPUID_TLB               2
#define CPUID_SERIALNUM         3
#define CPUID_EXTEND_FEATURE    7
#define CPUID_EXTEND_FUNCTION_1 0x80000001


enum cpuid_cache_idx {
	CPUID_VENDORSTRING_CACHE_IDX = 0,
	CPUID_FEATURES_CACHE_IDX,
	CPUID_EXTEND_FEATURE_CACHE_IDX,
	CPUID_EXTEND_FEATURE_CACHE_MAX
};

struct cpuid_cache_entry {
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t inited;
	uint32_t reserved;
};

static inline void native_cpuid_count(uint32_t op, uint32_t count,
	uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
	/* Execute CPUID instruction and save results */
	asm volatile("cpuid":"=a"(*a), "=b"(*b),
			"=c"(*c), "=d"(*d)
			: "a"(op), "c" (count));
}

void cpuid_count(uint32_t op, uint32_t count,
	uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d);

#define cpuid(op, a, b, c, d) cpuid_count(op, 0, a, b, c, d)

void emulate_cpuid(struct vcpu *vcpu, uint32_t src_op, uint32_t *eax_ptr,
	uint32_t *ebx_ptr, uint32_t *ecx_ptr, uint32_t *edx_ptr);

#endif /* CPUID_H_ */
