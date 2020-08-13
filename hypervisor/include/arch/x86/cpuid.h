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
#define CPUID_ECX_SSE3          (1U<<0U)
#define CPUID_ECX_PCLMUL        (1U<<1U)
#define CPUID_ECX_DTES64        (1U<<2U)
#define CPUID_ECX_MONITOR       (1U<<3U)
#define CPUID_ECX_DS_CPL        (1U<<4U)
#define CPUID_ECX_VMX           (1U<<5U)
#define CPUID_ECX_SMX           (1U<<6U)
#define CPUID_ECX_EST           (1U<<7U)
#define CPUID_ECX_TM2           (1U<<8U)
#define CPUID_ECX_SSSE3         (1U<<9U)
#define CPUID_ECX_CID           (1U<<10U)
#define CPUID_ECX_SDBG          (1U<<11U)
#define CPUID_ECX_FMA           (1U<<12U)
#define CPUID_ECX_CX16          (1U<<13U)
#define CPUID_ECX_ETPRD         (1U<<14U)
#define CPUID_ECX_PDCM          (1U<<15U)
#define CPUID_ECX_DCA           (1U<<18U)
#define CPUID_ECX_SSE4_1        (1U<<19U)
#define CPUID_ECX_SSE4_2        (1U<<20U)
#define CPUID_ECX_x2APIC        (1U<<21U)
#define CPUID_ECX_MOVBE         (1U<<22U)
#define CPUID_ECX_POPCNT        (1U<<23U)
#define CPUID_ECX_AES           (1U<<25U)
#define CPUID_ECX_XSAVE         (1U<<26U)
#define CPUID_ECX_OSXSAVE       (1U<<27U)
#define CPUID_ECX_AVX           (1U<<28U)
#define CPUID_ECX_HV            (1U<<31U)
#define CPUID_EDX_FPU           (1U<<0U)
#define CPUID_EDX_VME           (1U<<1U)
#define CPUID_EDX_DE            (1U<<2U)
#define CPUID_EDX_PSE           (1U<<3U)
#define CPUID_EDX_TSC           (1U<<4U)
#define CPUID_EDX_MSR           (1U<<5U)
#define CPUID_EDX_PAE           (1U<<6U)
#define CPUID_EDX_MCE           (1U<<7U)
#define CPUID_EDX_CX8           (1U<<8U)
#define CPUID_EDX_APIC          (1U<<9U)
#define CPUID_EDX_SEP           (1U<<11U)
#define CPUID_EDX_MTRR          (1U<<12U)
#define CPUID_EDX_PGE           (1U<<13U)
#define CPUID_EDX_MCA           (1U<<14U)
#define CPUID_EDX_CMOV          (1U<<15U)
#define CPUID_EDX_PAT           (1U<<16U)
#define CPUID_EDX_PSE36         (1U<<17U)
#define CPUID_EDX_PSN           (1U<<18U)
#define CPUID_EDX_CLF           (1U<<19U)
#define CPUID_EDX_DTES          (1U<<21U)
#define CPUID_EDX_ACPI          (1U<<22U)
#define CPUID_EDX_MMX           (1U<<23U)
#define CPUID_EDX_FXSR          (1U<<24U)
#define CPUID_EDX_SSE           (1U<<25U)
#define CPUID_EDX_SSE2          (1U<<26U)
#define CPUID_EDX_SS            (1U<<27U)
#define CPUID_EDX_HTT           (1U<<28U)
#define CPUID_EDX_TM1           (1U<<29U)
#define CPUID_EDX_IA64          (1U<<30U)
#define CPUID_EDX_PBE           (1U<<31U)
/* CPUID.07H:EBX.TSC_ADJUST*/
#define CPUID_EBX_TSC_ADJ       (1U<<1U)
/* CPUID.07H:EBX.SGX */
#define CPUID_EBX_SGX           (1U<<2U)
/* CPUID.07H:EBX.MPX */
#define CPUID_EBX_MPX           (1U<<14U)
/* CPUID.07H:ECX.CET_SS */
#define CPUID_ECX_CET_SS        (1U<<7U)
/* CPUID.07H:ECX.SGX_LC*/
#define CPUID_ECX_SGX_LC        (1U<<30U)
/* CPUID.07H:EDX.CET_IBT */
#define CPUID_EDX_CET_IBT       (1U<<20U)
/* CPUID.07H:EDX.IBRS_IBPB*/
#define CPUID_EDX_IBRS_IBPB     (1U<<26U)
/* CPUID.07H:EDX.STIBP*/
#define CPUID_EDX_STIBP         (1U<<27U)
/* CPUID.80000001H:EDX.Page1GB*/
#define CPUID_EDX_PAGE1GB       (1U<<26U)
/* CPUID.07H:EBX.INVPCID*/
#define CPUID_EBX_INVPCID       (1U<<10U)
/* CPUID.07H:EBX.PQM */
#define CPUID_EBX_PQM           (1U<<12U)
/* CPUID.07H:EBX.PQE */
#define CPUID_EBX_PQE           (1U<<15U)
/* CPUID.07H:EBX.INTEL_PROCESSOR_TRACE */
#define CPUID_EBX_PROC_TRC      (1U<<25U)
/* CPUID.01H:ECX.PCID*/
#define CPUID_ECX_PCID          (1U<<17U)
/* CPUID.0DH.EAX.XCR0_BNDREGS */
#define CPUID_EAX_XCR0_BNDREGS  (1U<<3U)
/* CPUID.0DH.EAX.XCR0_BNDCSR */
#define CPUID_EAX_XCR0_BNDCSR   (1U<<4U)
/* CPUID.0DH.ECX.CET_U_STATE */
#define CPUID_ECX_CET_U_STATE   (1U<<11U)
/* CPUID.0DH.ECX.CET_S_STATE */
#define CPUID_ECX_CET_S_STATE   (1U<<12U)
/* CPUID.12H.EAX.SGX1 */
#define CPUID_EAX_SGX1          (1U<<0U)
/* CPUID.12H.EAX.SGX2 */
#define CPUID_EAX_SGX2          (1U<<1U)
/* CPUID.80000001H.EDX.XD_BIT_AVAILABLE */
#define CPUID_EDX_XD_BIT_AVIL   (1U<<20U)

/* CPUID source operands */
#define CPUID_VENDORSTRING      0U
#define CPUID_FEATURES          1U
#define CPUID_TLB               2U
#define CPUID_SERIALNUM         3U
#define CPUID_EXTEND_FEATURE    7U
#define CPUID_XSAVE_FEATURES   0xDU
#define CPUID_RDT_ALLOCATION   0x10U
#define CPUID_MAX_EXTENDED_FUNCTION  0x80000000U
#define CPUID_EXTEND_FUNCTION_1      0x80000001U
#define CPUID_EXTEND_FUNCTION_2      0x80000002U
#define CPUID_EXTEND_FUNCTION_3      0x80000003U
#define CPUID_EXTEND_FUNCTION_4      0x80000004U
#define CPUID_EXTEND_INVA_TSC        0x80000007U
#define CPUID_EXTEND_ADDRESS_SIZE    0x80000008U

static inline void cpuid_subleaf(uint32_t leaf, uint32_t subleaf,
				uint32_t *eax, uint32_t *ebx,
				uint32_t *ecx, uint32_t *edx)
{
	/* Execute CPUID instruction and save results */
	asm volatile("cpuid":"=a"(*eax), "=b"(*ebx),
			"=c"(*ecx), "=d"(*edx)
			: "a" (leaf), "c" (subleaf)
			: "memory");
}

#endif /* CPUID_H_ */
