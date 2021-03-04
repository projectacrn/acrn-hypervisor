/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains pure vmx operations
 */

#include <types.h>
#include <x86/msr.h>
#include <x86/per_cpu.h>
#include <x86/pgtable.h>
#include <x86/vmx.h>

/**
 * @pre addr != NULL && addr is 4KB-aligned
 * rev[31:0]  32 bits located at vmxon region physical address
 * @pre rev[30:0] == VMCS revision && rev[31] == 0
 */
static inline void exec_vmxon(void *addr)
{
	/* Turn VMX on, pre-conditions can avoid VMfailInvalid
	 * here no need check RFLAGS since it will generate #GP or #UD
	 * except VMsuccess. SDM 30.3
	 */
	asm volatile (
			"vmxon (%%rax)\n"
			:
			: "a"(addr)
			: "cc", "memory");

}

/* Per cpu data to hold the vmxon_region for each pcpu.
 * It will be used again when we start a pcpu after the pcpu was down.
 * S3 enter/exit will use it.
 * Only run on current pcpu.
 */
void vmx_on(void)
{
	uint64_t tmp64;
	uint32_t tmp32;
	void *vmxon_region_va = (void *)get_cpu_var(vmxon_region);
	uint64_t vmxon_region_pa;

	/* Initialize vmxon page with revision id from IA32 VMX BASIC MSR */
	tmp32 = (uint32_t)msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s(vmxon_region_va, 4U, (void *)&tmp32, 4U);

	/* Turn on CR0.NE and CR4.VMXE */
	CPU_CR_READ(cr0, &tmp64);
	CPU_CR_WRITE(cr0, tmp64 | CR0_NE);
	CPU_CR_READ(cr4, &tmp64);
	CPU_CR_WRITE(cr4, tmp64 | CR4_VMXE);

	/* Read Feature ControL MSR */
	tmp64 = msr_read(MSR_IA32_FEATURE_CONTROL);

	/* Check if feature control is locked */
	if ((tmp64 & MSR_IA32_FEATURE_CONTROL_LOCK) == 0U) {
		/* Lock and enable VMX support */
		tmp64 |= (MSR_IA32_FEATURE_CONTROL_LOCK |
			  MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX);
		msr_write(MSR_IA32_FEATURE_CONTROL, tmp64);
	}

	/* Turn ON VMX */
	vmxon_region_pa = hva2hpa(vmxon_region_va);
	exec_vmxon(&vmxon_region_pa);
}

static inline void exec_vmxoff(void)
{
	asm volatile ("vmxoff" : : : "memory");
}

/**
 * @pre addr != NULL && addr is 4KB-aligned
 * @pre addr != VMXON pointer
 */
void exec_vmclear(void *addr)
{

	/* pre-conditions can avoid VMfail
	 * here no need check RFLAGS since it will generate #GP or #UD
	 * except VMsuccess. SDM 30.3
	 */
	asm volatile (
		"vmclear (%%rax)\n"
		:
		: "a"(addr)
		: "cc", "memory");
}

/**
 * @pre addr != NULL && addr is 4KB-aligned
 * @pre addr != VMXON pointer
 */
void exec_vmptrld(void *addr)
{
	/* pre-conditions can avoid VMfail
	 * here no need check RFLAGS since it will generate #GP or #UD
	 * except VMsuccess. SDM 30.3
	 */
	asm volatile (
		"vmptrld (%%rax)\n"
		:
		: "a"(addr)
		: "cc", "memory");
}

/**
 * only run on current pcpu
 */
void vmx_off(void)
{
	void **vmcs_ptr = &get_cpu_var(vmcs_run);

	if (*vmcs_ptr != NULL) {
		uint64_t vmcs_pa;

		vmcs_pa = hva2hpa(*vmcs_ptr);
		exec_vmclear((void *)&vmcs_pa);
		*vmcs_ptr = NULL;
	}

	exec_vmxoff();
}

uint64_t exec_vmread64(uint32_t field_full)
{
	uint64_t value;

	asm volatile (
		"vmread %%rdx, %%rax "
		: "=a" (value)
		: "d"(field_full)
		: "cc");

	return value;
}

uint32_t exec_vmread32(uint32_t field)
{
	uint64_t value;

	value = exec_vmread64(field);

	return (uint32_t)value;
}

uint16_t exec_vmread16(uint32_t field)
{
        uint64_t value;

        value = exec_vmread64(field);

        return (uint16_t)value;
}

void exec_vmwrite64(uint32_t field_full, uint64_t value)
{
	asm volatile (
		"vmwrite %%rax, %%rdx "
		: : "a" (value), "d"(field_full)
		: "cc");
}

void exec_vmwrite32(uint32_t field, uint32_t value)
{
	exec_vmwrite64(field, (uint64_t)value);
}

void exec_vmwrite16(uint32_t field, uint16_t value)
{
	exec_vmwrite64(field, (uint64_t)value);
}
