/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

static void set_tss_desc(union tss_64_descriptor *desc,
		void *tss, int tss_limit, int type)
{
	uint32_t u1, u2, u3;

	u1 = ((uint64_t)tss << 16) & 0xFFFFFFFF;
	u2 = (uint64_t)tss & 0xFF000000;
	u3 = ((uint64_t)tss & 0x00FF0000) >> 16;


	desc->fields.low32.value = u1 | (tss_limit & 0xFFFF);
	desc->fields.base_addr_63_32 = (uint32_t)((uint64_t)tss >> 32);
	desc->fields.high32.value = (u2 | ((uint32_t)type << 8) | 0x8000 | u3);
}

void load_gdtr_and_tr(void)
{
	struct host_gdt *gdt = &get_cpu_var(gdt);
	struct host_gdt_descriptor gdtr;
	struct tss_64 *tss = &get_cpu_var(tss);

	/* first entry is not used */
	gdt->rsvd = 0xAAAAAAAAAAAAAAAA;
	/* ring 0 code sel descriptor */
	gdt->host_gdt_code_descriptor.value = 0x00Af9b000000ffff;
	/* ring 0 data sel descriptor */
	gdt->host_gdt_data_descriptor.value = 0x00cf93000000ffff;

	tss->ist1 = (uint64_t)get_cpu_var(mc_stack) + STACK_SIZE;
	tss->ist2 = (uint64_t)get_cpu_var(df_stack) + STACK_SIZE;
	tss->ist3 = (uint64_t)get_cpu_var(sf_stack) + STACK_SIZE;
	tss->ist4 = 0L;

	/* tss descriptor */
	set_tss_desc(&gdt->host_gdt_tss_descriptors,
		(void *)tss, sizeof(struct tss_64), TSS_AVAIL);

	gdtr.len = sizeof(struct host_gdt) - 1;
	gdtr.gdt = gdt;

	asm volatile ("lgdt %0" ::"m"(gdtr));

	CPU_LTR_EXECUTE(HOST_GDT_RING0_CPU_TSS_SEL);
}
