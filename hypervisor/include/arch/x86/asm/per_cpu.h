/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PER_CPU_X86_H
#define PER_CPU_X86_H

#include <types.h>
#include <asm/page.h>
#include <profiling.h>
#include <asm/gdt.h>
#include <asm/security.h>
#include <asm/vm_config.h>

struct per_cpu_arch {
	/* vmxon_region MUST be 4KB-aligned */
	uint8_t vmxon_region[PAGE_SIZE];
	void *vmcs_run;
	struct host_gdt gdt;
	struct tss_64 tss;
	uint8_t mc_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t df_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint8_t sf_stack[CONFIG_STACK_SIZE] __aligned(16);
	uint32_t lapic_id;
	uint32_t lapic_ldr;
#ifdef PROFILING_ON
	struct profiling_info_wrapper profiling_info;
#endif
	uint64_t tsc_suspend;
	struct acrn_vcpu *whose_iwkey;

} __aligned(PAGE_SIZE); /* per_cpu_region size aligned with PAGE_SIZE */


#endif
