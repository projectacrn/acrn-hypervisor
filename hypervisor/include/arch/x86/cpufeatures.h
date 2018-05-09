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

#ifndef __X86_CPUFEATURES_H__
#define __X86_CPUFEATURES_H__

/* Intel-defined CPU features, CPUID level 0x00000001 (ECX)*/
#define X86_FEATURE_SSE3	((FEAT_1_ECX << 5) +  0)
#define X86_FEATURE_PCLMUL	((FEAT_1_ECX << 5) +  1)
#define X86_FEATURE_DTES64	((FEAT_1_ECX << 5) +  2)
#define X86_FEATURE_MONITOR	((FEAT_1_ECX << 5) +  3)
#define X86_FEATURE_DS_CPL	((FEAT_1_ECX << 5) +  4)
#define X86_FEATURE_VMX		((FEAT_1_ECX << 5) +  5)
#define X86_FEATURE_SMX		((FEAT_1_ECX << 5) +  6)
#define X86_FEATURE_EST		((FEAT_1_ECX << 5) +  7)
#define X86_FEATURE_TM2		((FEAT_1_ECX << 5) +  8)
#define X86_FEATURE_SSSE3	((FEAT_1_ECX << 5) +  9)
#define X86_FEATURE_CID		((FEAT_1_ECX << 5) + 10)
#define X86_FEATURE_FMA		((FEAT_1_ECX << 5) + 12)
#define X86_FEATURE_CX16	((FEAT_1_ECX << 5) + 13)
#define X86_FEATURE_ETPRD	((FEAT_1_ECX << 5) + 14)
#define X86_FEATURE_PDCM	((FEAT_1_ECX << 5) + 15)
#define X86_FEATURE_PCID	((FEAT_1_ECX << 5) + 17)
#define X86_FEATURE_DCA		((FEAT_1_ECX << 5) + 18)
#define X86_FEATURE_SSE4_1	((FEAT_1_ECX << 5) + 19)
#define X86_FEATURE_SSE4_2	((FEAT_1_ECX << 5) + 20)
#define X86_FEATURE_x2APIC	((FEAT_1_ECX << 5) + 21)
#define X86_FEATURE_MOVBE	((FEAT_1_ECX << 5) + 22)
#define X86_FEATURE_POPCNT	((FEAT_1_ECX << 5) + 23)
#define X86_FEATURE_TSC_DEADLINE	((FEAT_1_ECX << 5) + 24)
#define X86_FEATURE_AES		((FEAT_1_ECX << 5) + 25)
#define X86_FEATURE_XSAVE	((FEAT_1_ECX << 5) + 26)
#define X86_FEATURE_OSXSAVE	((FEAT_1_ECX << 5) + 27)
#define X86_FEATURE_AVX		((FEAT_1_ECX << 5) + 28)

/* Intel-defined CPU features, CPUID level 0x00000001 (EDX)*/
#define X86_FEATURE_FPU		((FEAT_1_EDX << 5) +  0)
#define X86_FEATURE_VME		((FEAT_1_EDX << 5) +  1)
#define X86_FEATURE_DE		((FEAT_1_EDX << 5) +  2)
#define X86_FEATURE_PSE		((FEAT_1_EDX << 5) +  3)
#define X86_FEATURE_TSC		((FEAT_1_EDX << 5) +  4)
#define X86_FEATURE_MSR		((FEAT_1_EDX << 5) +  5)
#define X86_FEATURE_PAE		((FEAT_1_EDX << 5) +  6)
#define X86_FEATURE_MCE		((FEAT_1_EDX << 5) +  7)
#define X86_FEATURE_CX8		((FEAT_1_EDX << 5) +  8)
#define X86_FEATURE_APIC	((FEAT_1_EDX << 5) +  9)
#define X86_FEATURE_SEP		((FEAT_1_EDX << 5) + 11)
#define X86_FEATURE_MTRR	((FEAT_1_EDX << 5) + 12)
#define X86_FEATURE_PGE		((FEAT_1_EDX << 5) + 13)
#define X86_FEATURE_MCA		((FEAT_1_EDX << 5) + 14)
#define X86_FEATURE_CMOV	((FEAT_1_EDX << 5) + 15)
#define X86_FEATURE_PAT		((FEAT_1_EDX << 5) + 16)
#define X86_FEATURE_PSE36	((FEAT_1_EDX << 5) + 17)
#define X86_FEATURE_PSN		((FEAT_1_EDX << 5) + 18)
#define X86_FEATURE_CLF		((FEAT_1_EDX << 5) + 19)
#define X86_FEATURE_DTES	((FEAT_1_EDX << 5) + 21)
#define X86_FEATURE_ACPI	((FEAT_1_EDX << 5) + 22)
#define X86_FEATURE_MMX		((FEAT_1_EDX << 5) + 23)
#define X86_FEATURE_FXSR	((FEAT_1_EDX << 5) + 24)
#define X86_FEATURE_SSE		((FEAT_1_EDX << 5) + 25)
#define X86_FEATURE_SSE2	((FEAT_1_EDX << 5) + 26)
#define X86_FEATURE_SS		((FEAT_1_EDX << 5) + 27)
#define X86_FEATURE_HTT		((FEAT_1_EDX << 5) + 28)
#define X86_FEATURE_TM1		((FEAT_1_EDX << 5) + 29)
#define X86_FEATURE_IA64	((FEAT_1_EDX << 5) + 30)
#define X86_FEATURE_PBE		((FEAT_1_EDX << 5) + 31)

/* Intel-defined CPU features, CPUID level 0x00000007 (EBX)*/
#define X86_FEATURE_TSC_ADJ	((FEAT_7_0_EBX << 5) +  1)
#define X86_FEATURE_SMEP	((FEAT_7_0_EBX << 5) +  7)
#define X86_FEATURE_INVPCID	((FEAT_7_0_EBX << 5) + 10)
#define X86_FEATURE_SMAP	((FEAT_7_0_EBX << 5) + 20)

/* Intel-defined CPU features, CPUID level 0x00000007 (EDX)*/
#define X86_FEATURE_IBRS_IBPB	((FEAT_7_0_EDX << 5) + 26)
#define X86_FEATURE_STIBP	((FEAT_7_0_EDX << 5) + 27)

/* Intel-defined CPU features, CPUID level 0x80000001 (EDX)*/
#define X86_FEATURE_NX		((FEAT_8000_0001_EDX << 5) + 20)
#define X86_FEATURE_PAGE1GB	((FEAT_8000_0001_EDX << 5) + 26)
#define X86_FEATURE_LM		((FEAT_8000_0001_EDX << 5) + 29)

#endif /*__X86_CPUFEATURES_H__*/
