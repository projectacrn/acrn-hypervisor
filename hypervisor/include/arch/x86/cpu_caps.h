/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUINFO_H
#define CPUINFO_H

#define MAX_PSTATE	20U	/* max num of supported Px count */
#define MAX_CSTATE	8U	/* max num of supported Cx count */
/* We support MAX_CSTATE num of Cx, means have (MAX_CSTATE - 1) Cx entries,
 * i.e. supported Cx entry index range from 1 to MAX_CX_ENTRY.
 */
#define MAX_CX_ENTRY	(MAX_CSTATE - 1U)

/* CPUID feature words */
#define	FEAT_1_ECX		0U     /* CPUID[1].ECX */
#define	FEAT_1_EDX		1U     /* CPUID[1].EDX */
#define	FEAT_7_0_EBX		2U     /* CPUID[EAX=7,ECX=0].EBX */
#define	FEAT_7_0_ECX		3U     /* CPUID[EAX=7,ECX=0].ECX */
#define	FEAT_7_0_EDX		4U     /* CPUID[EAX=7,ECX=0].EDX */
#define	FEAT_8000_0001_ECX	5U     /* CPUID[8000_0001].ECX */
#define	FEAT_8000_0001_EDX	6U     /* CPUID[8000_0001].EDX */
#define	FEAT_8000_0007_EDX	7U     /* CPUID[8000_0007].EDX */
#define	FEAT_8000_0008_EBX	8U     /* CPUID[8000_0008].EBX */
#define	FEAT_D_0_EAX		9U     /* CPUID[D][0].EAX */
#define	FEAT_D_0_EDX		10U    /* CPUID[D][0].EDX */
#define	FEAT_D_1_EAX		11U    /* CPUID[D][1].EAX */
#define	FEAT_D_1_ECX		13U    /* CPUID[D][1].ECX */
#define	FEAT_D_1_EDX		14U    /* CPUID[D][1].EDX */
#define	FEATURE_WORDS		15U

struct cpuinfo_x86 {
	/* SDM 2-2 Vol.4 Table 2-1 uses DisplayFamily_DisplayModel to
	 * distinguish Processor Families/Processor Number Series.
	 */
	uint8_t displayfamily, displaymodel;
	uint8_t virt_bits;
	uint8_t phys_bits;
	uint32_t cpuid_level;
	uint32_t extended_cpuid_level;
	uint64_t physical_address_mask;
	uint32_t cpuid_leaves[FEATURE_WORDS];
	char model_name[64];
};

bool has_monitor_cap(void);
bool is_apl_platform(void);
bool is_apicv_advanced_feature_supported(void);
bool pcpu_has_cap(uint32_t bit);
bool pcpu_has_vmx_ept_cap(uint32_t bit_mask);
bool pcpu_has_vmx_vpid_cap(uint32_t bit_mask);
bool is_apl_platform(void);
bool has_core_cap(uint32_t bit_mask);
void init_pcpu_capabilities(void);
void init_pcpu_model_name(void);
int32_t detect_hardware_support(void);
struct cpuinfo_x86 *get_pcpu_info(void);

#endif /* CPUINFO_H */
