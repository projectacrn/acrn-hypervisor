/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUFEATURES_H
#define CPUFEATURES_H

/* Intel-defined CPU features, CPUID level 0x00000001 (ECX)*/
#define X86_FEATURE_SSE3	((FEAT_1_ECX << 5U) +  0U)
#define X86_FEATURE_PCLMUL	((FEAT_1_ECX << 5U) +  1U)
#define X86_FEATURE_DTES64	((FEAT_1_ECX << 5U) +  2U)
#define X86_FEATURE_MONITOR	((FEAT_1_ECX << 5U) +  3U)
#define X86_FEATURE_DS_CPL	((FEAT_1_ECX << 5U) +  4U)
#define X86_FEATURE_VMX		((FEAT_1_ECX << 5U) +  5U)
#define X86_FEATURE_SMX		((FEAT_1_ECX << 5U) +  6U)
#define X86_FEATURE_EST		((FEAT_1_ECX << 5U) +  7U)
#define X86_FEATURE_TM2		((FEAT_1_ECX << 5U) +  8U)
#define X86_FEATURE_SSSE3	((FEAT_1_ECX << 5U) +  9U)
#define X86_FEATURE_CID		((FEAT_1_ECX << 5U) + 10U)
#define X86_FEATURE_FMA		((FEAT_1_ECX << 5U) + 12U)
#define X86_FEATURE_CX16	((FEAT_1_ECX << 5U) + 13U)
#define X86_FEATURE_ETPRD	((FEAT_1_ECX << 5U) + 14U)
#define X86_FEATURE_PDCM	((FEAT_1_ECX << 5U) + 15U)
#define X86_FEATURE_PCID	((FEAT_1_ECX << 5U) + 17U)
#define X86_FEATURE_DCA		((FEAT_1_ECX << 5U) + 18U)
#define X86_FEATURE_SSE4_1	((FEAT_1_ECX << 5U) + 19U)
#define X86_FEATURE_SSE4_2	((FEAT_1_ECX << 5U) + 20U)
#define X86_FEATURE_X2APIC	((FEAT_1_ECX << 5U) + 21U)
#define X86_FEATURE_MOVBE	((FEAT_1_ECX << 5U) + 22U)
#define X86_FEATURE_POPCNT	((FEAT_1_ECX << 5U) + 23U)
#define X86_FEATURE_TSC_DEADLINE	((FEAT_1_ECX << 5U) + 24U)
#define X86_FEATURE_AES		((FEAT_1_ECX << 5U) + 25U)
#define X86_FEATURE_XSAVE	((FEAT_1_ECX << 5U) + 26U)
#define X86_FEATURE_OSXSAVE	((FEAT_1_ECX << 5U) + 27U)
#define X86_FEATURE_AVX		((FEAT_1_ECX << 5U) + 28U)

/* Intel-defined CPU features, CPUID level 0x00000001 (EDX)*/
#define X86_FEATURE_FPU		((FEAT_1_EDX << 5U) +  0U)
#define X86_FEATURE_VME		((FEAT_1_EDX << 5U) +  1U)
#define X86_FEATURE_DE		((FEAT_1_EDX << 5U) +  2U)
#define X86_FEATURE_PSE		((FEAT_1_EDX << 5U) +  3U)
#define X86_FEATURE_TSC		((FEAT_1_EDX << 5U) +  4U)
#define X86_FEATURE_MSR		((FEAT_1_EDX << 5U) +  5U)
#define X86_FEATURE_PAE		((FEAT_1_EDX << 5U) +  6U)
#define X86_FEATURE_MCE		((FEAT_1_EDX << 5U) +  7U)
#define X86_FEATURE_CX8		((FEAT_1_EDX << 5U) +  8U)
#define X86_FEATURE_APIC	((FEAT_1_EDX << 5U) +  9U)
#define X86_FEATURE_SEP		((FEAT_1_EDX << 5U) + 11U)
#define X86_FEATURE_MTRR	((FEAT_1_EDX << 5U) + 12U)
#define X86_FEATURE_PGE		((FEAT_1_EDX << 5U) + 13U)
#define X86_FEATURE_MCA		((FEAT_1_EDX << 5U) + 14U)
#define X86_FEATURE_CMOV	((FEAT_1_EDX << 5U) + 15U)
#define X86_FEATURE_PAT		((FEAT_1_EDX << 5U) + 16U)
#define X86_FEATURE_PSE36	((FEAT_1_EDX << 5U) + 17U)
#define X86_FEATURE_PSN		((FEAT_1_EDX << 5U) + 18U)
#define X86_FEATURE_CLF		((FEAT_1_EDX << 5U) + 19U)
#define X86_FEATURE_DTES	((FEAT_1_EDX << 5U) + 21U)
#define X86_FEATURE_ACPI	((FEAT_1_EDX << 5U) + 22U)
#define X86_FEATURE_MMX		((FEAT_1_EDX << 5U) + 23U)
#define X86_FEATURE_FXSR	((FEAT_1_EDX << 5U) + 24U)
#define X86_FEATURE_SSE		((FEAT_1_EDX << 5U) + 25U)
#define X86_FEATURE_SSE2	((FEAT_1_EDX << 5U) + 26U)
#define X86_FEATURE_SS		((FEAT_1_EDX << 5U) + 27U)
#define X86_FEATURE_HTT		((FEAT_1_EDX << 5U) + 28U)
#define X86_FEATURE_TM1		((FEAT_1_EDX << 5U) + 29U)
#define X86_FEATURE_IA64	((FEAT_1_EDX << 5U) + 30U)
#define X86_FEATURE_PBE		((FEAT_1_EDX << 5U) + 31U)

/* Intel-defined CPU features, CPUID level 0x00000007 (EBX)*/
#define X86_FEATURE_TSC_ADJ	((FEAT_7_0_EBX << 5U) +  1U)
#define X86_FEATURE_SMEP	((FEAT_7_0_EBX << 5U) +  7U)
#define X86_FEATURE_ERMS	((FEAT_7_0_EBX << 5U) +  9U)
#define X86_FEATURE_INVPCID	((FEAT_7_0_EBX << 5U) + 10U)
#define X86_FEATURE_CAT        ((FEAT_7_0_EBX << 5U) + 15U)
#define X86_FEATURE_SMAP	((FEAT_7_0_EBX << 5U) + 20U)

/* Intel-defined CPU features, CPUID level 0x00000007 (EDX)*/
#define X86_FEATURE_IBRS_IBPB	((FEAT_7_0_EDX << 5U) + 26U)
#define X86_FEATURE_STIBP	((FEAT_7_0_EDX << 5U) + 27U)
#define X86_FEATURE_L1D_FLUSH	((FEAT_7_0_EDX << 5U) + 28U)
#define X86_FEATURE_ARCH_CAP	((FEAT_7_0_EDX << 5U) + 29U)

/* Intel-defined CPU features, CPUID level 0x80000001 (EDX)*/
#define X86_FEATURE_NX		((FEAT_8000_0001_EDX << 5U) + 20U)
#define X86_FEATURE_PAGE1GB	((FEAT_8000_0001_EDX << 5U) + 26U)
#define X86_FEATURE_LM		((FEAT_8000_0001_EDX << 5U) + 29U)

#endif /* CPUFEATURES_H */
