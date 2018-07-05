/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static void set_tss_desc(union tss_64_descriptor *desc,
		void *tss, size_t tss_limit, int type)
{
	uint32_t u1, u2, u3;

	u1 = (uint32_t)(((uint64_t)tss << 16U) & 0xFFFFFFFFU);
	u2 = (uint32_t)((uint64_t)tss & 0xFF000000U);
	u3 = (uint32_t)(((uint64_t)tss & 0x00FF0000U) >> 16U);


	desc->fields.low32.value = u1 | (tss_limit & 0xFFFFU);
	desc->fields.base_addr_63_32 = (uint32_t)((uint64_t)tss >> 32U);
	desc->fields.high32.value = (u2 | ((uint32_t)type << 8U) | 0x8000U | u3);
}

void load_gdtr_and_tr(void)
{
	struct host_gdt *gdt = &get_cpu_var(gdt);
	struct host_gdt_descriptor gdtr;
	struct tss_64 *tss = &get_cpu_var(tss);

	/* first entry is not used */
	gdt->rsvd = 0xAAAAAAAAAAAAAAAAUL;
	/* ring 0 code sel descriptor */
	gdt->host_gdt_code_descriptor.value = 0x00Af9b000000ffffUL;
	/* ring 0 data sel descriptor */
	gdt->host_gdt_data_descriptor.value = 0x00cf93000000ffffUL;

	tss->ist1 = (uint64_t)get_cpu_var(mc_stack) + CONFIG_STACK_SIZE;
	tss->ist2 = (uint64_t)get_cpu_var(df_stack) + CONFIG_STACK_SIZE;
	tss->ist3 = (uint64_t)get_cpu_var(sf_stack) + CONFIG_STACK_SIZE;
	tss->ist4 = 0UL;

	/* tss descriptor */
	set_tss_desc(&gdt->host_gdt_tss_descriptors,
		(void *)tss, sizeof(struct tss_64), TSS_AVAIL);

	gdtr.len = sizeof(struct host_gdt) - 1U;
	gdtr.gdt = gdt;

	asm volatile ("lgdt %0" ::"m"(gdtr));

	CPU_LTR_EXECUTE(HOST_GDT_RING0_CPU_TSS_SEL);
}
