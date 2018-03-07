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

#include <hv_lib.h>
#include <cpu.h>
#include <gdt.h>

DEFINE_CPU_DATA(struct tss_64, tss);
DEFINE_CPU_DATA(struct host_gdt, gdt);
DEFINE_CPU_DATA(uint8_t[STACK_SIZE], mc_stack) __aligned(16);
DEFINE_CPU_DATA(uint8_t[STACK_SIZE], df_stack) __aligned(16);
DEFINE_CPU_DATA(uint8_t[STACK_SIZE], sf_stack) __aligned(16);

static void set_tss_desc(union tss_64_descriptor *desc,
		void *tss, int tss_limit, int type)
{
	uint32_t u1, u2, u3;

	u1 = ((uint64_t)tss << 16) & 0xFFFFFFFF;
	u2 = (uint64_t)tss & 0xFF000000;
	u3 = ((uint64_t)tss & 0x00FF0000) >> 16;


	desc->low32.value = u1 | (tss_limit & 0xFFFF);
	desc->base_addr_63_32 = (uint32_t)((uint64_t)tss >> 32);
	desc->high32.value = (u2 | ((uint32_t)type << 8) | 0x8000 | u3);
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
