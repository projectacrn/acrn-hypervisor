/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/gdt.h>
#include <x86/per_cpu.h>

static void set_tss_desc(struct tss_64_descriptor *desc,
		uint64_t tss, size_t tss_limit, uint32_t type)
{
	uint32_t u1, u2, u3;
	uint32_t tss_hi_32 = (uint32_t)(tss >> 32U);
	uint32_t tss_lo_32 = (uint32_t)tss;

	u1 = tss_lo_32 << 16U;
	u2 = tss_lo_32 & 0xFF000000U;
	u3 = (tss_lo_32 & 0x00FF0000U) >> 16U;


	desc->low32_value = u1 | (tss_limit & 0xFFFFU);
	desc->base_addr_63_32 = tss_hi_32;
	desc->high32_value = u2 | (type << 8U) | 0x8000U | u3;
}

static inline void load_gdt(struct host_gdt_descriptor *gdtr)
{
	asm volatile ("lgdt %0" ::"m"(*gdtr));
}

void load_gdtr_and_tr(void)
{
	struct host_gdt *gdt = &get_cpu_var(gdt);
	struct host_gdt_descriptor gdtr;
	struct tss_64 *tss = &get_cpu_var(tss);

	/* first entry is not used */
	gdt->rsvd = 0xAAAAAAAAAAAAAAAAUL;
	/* ring 0 code sel descriptor */
	gdt->code_segment_descriptor = 0x00Af9b000000ffffUL;
	/* ring 0 data sel descriptor */
	gdt->data_segment_descriptor = 0x00cf93000000ffffUL;

	tss->ist1 = (uint64_t)get_cpu_var(mc_stack) + CONFIG_STACK_SIZE;
	tss->ist2 = (uint64_t)get_cpu_var(df_stack) + CONFIG_STACK_SIZE;
	tss->ist3 = (uint64_t)get_cpu_var(sf_stack) + CONFIG_STACK_SIZE;
	tss->ist4 = 0UL;

	/* tss descriptor */
	set_tss_desc(&gdt->host_gdt_tss_descriptors,
		(uint64_t)tss, sizeof(struct tss_64), TSS_AVAIL);

	gdtr.len = sizeof(struct host_gdt) - 1U;
	gdtr.gdt = gdt;

	load_gdt(&gdtr);

	CPU_LTR_EXECUTE(HOST_GDT_RING0_CPU_TSS_SEL);
}
