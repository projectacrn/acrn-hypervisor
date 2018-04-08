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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#ifdef CONFIG_EFI_STUB
#include <acrn_efi.h>
extern struct efi_ctx* efi_ctx;
#endif

#define PAT_POWER_ON_VALUE	(PAT_MEM_TYPE_WB + \
				((uint64_t)PAT_MEM_TYPE_WT << 8) + \
				((uint64_t)PAT_MEM_TYPE_UCM << 16) + \
				((uint64_t)PAT_MEM_TYPE_UC << 24) + \
				((uint64_t)PAT_MEM_TYPE_WB << 32) + \
				((uint64_t)PAT_MEM_TYPE_WT << 40) + \
				((uint64_t)PAT_MEM_TYPE_UCM << 48) + \
				((uint64_t)PAT_MEM_TYPE_UC << 56))

static inline int exec_vmxon(void *addr)
{
	uint64_t rflags;
	uint64_t tmp64;
	int status = 0;

	if (addr == NULL)
		status = -EINVAL;
	ASSERT(status == 0, "Incorrect arguments");

	/* Read Feature ControL MSR */
	tmp64 = msr_read(MSR_IA32_FEATURE_CONTROL);

	/* Determine if feature control is locked */
	if (tmp64 & MSR_IA32_FEATURE_CONTROL_LOCK) {
		/* See if VMX enabled */
		if (!(tmp64 & MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX)) {
			/* Return error - VMX can't be enabled */
			status = -EINVAL;
		}
	} else {
		/* Lock and enable VMX support */
		tmp64 |= (MSR_IA32_FEATURE_CONTROL_LOCK |
			  MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX);
		msr_write(MSR_IA32_FEATURE_CONTROL, tmp64);
	}

	/* Ensure previous operations successful */
	if (status == 0) {
		/* Turn VMX on */
		asm volatile ("mov %1, %%rax\n"
			      "vmxon (%%rax)\n"
			      "pushfq\n"
			      "pop %0\n":"=r" (rflags)
			      : "r"(addr)
			      : "%rax");

		/* if carry and zero flags are clear operation success */
		if (rflags & (RFLAGS_C | RFLAGS_Z))
			status = -EINVAL;
	}

	/* Return result to caller */
	return status;
}

int exec_vmxon_instr(void)
{
	uint64_t tmp64;
	uint32_t tmp32;
	int ret_val = -EINVAL;
	void *vmxon_region;

	/* Allocate page aligned memory for VMXON region */
	vmxon_region = alloc_page();

	if (vmxon_region != 0) {
		/* Initialize vmxon page with revision id from IA32 VMX BASIC
		 * MSR
		 */
		tmp32 = msr_read(MSR_IA32_VMX_BASIC);
		memcpy_s((uint32_t *) vmxon_region, 4, &tmp32, 4);

		/* Turn on CR0.NE and CR4.VMXE */
		CPU_CR_READ(cr0, &tmp64);
		CPU_CR_WRITE(cr0, tmp64 | CR0_NE);
		CPU_CR_READ(cr4, &tmp64);
		CPU_CR_WRITE(cr4, tmp64 | CR4_VMXE);

		/* Turn ON VMX */
		ret_val = exec_vmxon(&vmxon_region);
	}

	return ret_val;
}

int exec_vmclear(void *addr)
{
	uint64_t rflags;
	int status = 0;

	if (addr == NULL)
		status = -EINVAL;
	ASSERT(status == 0, "Incorrect arguments");

	asm volatile (
		"mov %1, %%rax\n"
		"vmclear (%%rax)\n"
		"pushfq\n"
		"pop %0\n":"=r" (rflags)
		: "r"(addr)
		: "%rax");

	/* if carry and zero flags are clear operation success */
	if (rflags & (RFLAGS_C | RFLAGS_Z))
		status = -EINVAL;

	return status;
}

int exec_vmptrld(void *addr)
{
	uint64_t rflags;
	int status = 0;

	if (addr == NULL)
		status = -EINVAL;
	ASSERT(status == 0, "Incorrect arguments");

	asm volatile (
		"mov %1, %%rax\n"
		"vmptrld (%%rax)\n"
		"pushfq\n"
		"pop %0\n"
		: "=r" (rflags)
		: "r"(addr)
		: "%rax");

	/* if carry and zero flags are clear operation success */
	if (rflags & (RFLAGS_C | RFLAGS_Z))
		status = -EINVAL;

	return status;
}

uint64_t exec_vmread(uint32_t field)
{
	uint64_t value;

	asm volatile (
		"vmread %%rdx, %%rax "
		: "=a" (value)
		: "d"(field)
		: "cc");

	return value;
}

uint64_t exec_vmread64(uint32_t field_full)
{
	uint64_t low;

	low = exec_vmread(field_full);

#ifdef __i386__
	low += exec_vmread(field_full + 1) << 32;
#endif
	return low;
}

void exec_vmwrite(uint32_t field, uint64_t value)
{
	asm volatile (
		"vmwrite %%rax, %%rdx "
		: : "a" (value), "d"(field)
		: "cc");
}

void exec_vmwrite64(unsigned int field_full, uint64_t value)
{
#ifdef __i386__
	int low = (int)(value & 0xFFFFFFFF);
	int high = (int)((value >> 32) & 0xFFFFFFFF);

	exec_vmwrite(field_full, low);
	exec_vmwrite(field_full + 1, high);
#else
	exec_vmwrite(field_full, value);
#endif
}

#define HV_ARCH_VMX_GET_CS(SEL)				\
{							\
	asm volatile ("movw %%cs, %%ax" : "=a"(sel));	\
}

uint32_t get_cs_access_rights(void)
{
	uint32_t usable_ar;
	uint16_t sel_value;

	asm volatile ("movw %%cs, %%ax" : "=a" (sel_value));
	asm volatile ("lar %%eax, %%eax" : "=a" (usable_ar) : "a"(sel_value));
	usable_ar = usable_ar >> 8;
	usable_ar &= 0xf0ff;	/* clear bits 11:8 */

	return usable_ar;
}

static void init_guest_state(struct vcpu *vcpu)
{
	uint64_t field;
	uint64_t value;
	uint32_t value32;
	uint64_t value64;
	uint16_t sel;
	uint32_t limit, access, base;
	uint32_t ldt_idx = 0x38;
	int es = 0, ss = 0, ds = 0, fs = 0, gs = 0, data32_idx;
	uint32_t lssd32_idx = 0x70;
	struct vm *vm = vcpu->vm;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	pr_dbg("*********************");
	pr_dbg("Initialize guest state");
	pr_dbg("*********************");

	/*************************************************/
	/* Set up CRx                                    */
	/*************************************************/
	pr_dbg("Natural-width********");

	/* Setup guest control register values */
	/* Set up guest CRO field */
	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		/*cur_context->cr0 = (CR0_CD | CR0_NW | CR0_ET | CR0_NE);*/
		cur_context->cr0 = CR0_ET | CR0_NE;
		cur_context->cr3 = 0;
		cur_context->cr4 = CR4_VMXE;
	} else if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE) {
		cur_context->cr0 = ((uint64_t)CR0_PG | CR0_PE | CR0_NE);
		cur_context->cr4 = ((uint64_t)CR4_PSE | CR4_PAE | CR4_MCE | CR4_VMXE);
		cur_context->cr3 = (uint64_t)vm->arch_vm.guest_pml4 | CR3_PWT;
	}

	value = cur_context->cr0;
	field = VMX_GUEST_CR0;
	exec_vmwrite(field, value & 0xFFFFFFFF);
	pr_dbg("VMX_GUEST_CR0: 0x%016llx ", value);

	/* Set up guest CR3 field */
	value = cur_context->cr3;
	field = VMX_GUEST_CR3;
	exec_vmwrite(field, value & 0xFFFFFFFF);
	pr_dbg("VMX_GUEST_CR3: 0x%016llx ", value);

	/* Set up guest CR4 field */
	value = cur_context->cr4;
	field = VMX_GUEST_CR4;
	exec_vmwrite(field, value & 0xFFFFFFFF);
	pr_dbg("VMX_GUEST_CR4: 0x%016llx ", value);

	/***************************************************/
	/* Set up Flags - the value of RFLAGS on VM entry */
	/***************************************************/
	field = VMX_GUEST_RFLAGS;
	cur_context->rflags = 0x2; 	/* Bit 1 is a active high reserved bit */
	exec_vmwrite(field, cur_context->rflags);
	pr_dbg("VMX_GUEST_RFLAGS: 0x%016llx ", value);

	/***************************************************/
	/* Set Code Segment - CS */
	/***************************************************/
	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		/* AP is initialized with real mode
		 * and CS value is left shift 8 bits from sipi vector;
		 */
		sel = vcpu->arch_vcpu.sipi_vector << 8;
		limit = 0xffff;
		access = 0x9F;
		base = sel << 4;
	} else {
		HV_ARCH_VMX_GET_CS(sel);
		access = get_cs_access_rights();
		limit = 0xffffffff;
		base = 0;
	}

	/* Selector */
	field = VMX_GUEST_CS_SEL;
	exec_vmwrite(field, sel);
	pr_dbg("VMX_GUEST_CS_SEL: 0x%x ", sel);

	/* Limit */
	field = VMX_GUEST_CS_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_CS_LIMIT: 0x%x ", limit);

	/* Access */
	field = VMX_GUEST_CS_ATTR;
	exec_vmwrite(field, access);
	pr_dbg("VMX_GUEST_CS_ATTR: 0x%x ", access);

	/* Base */
	field = VMX_GUEST_CS_BASE;
	exec_vmwrite(field, base);
	pr_dbg("VMX_GUEST_CS_BASE: 0x%016llx ", base);

	/***************************************************/
	/* Set up instruction pointer and stack pointer */
	/***************************************************/
	/* Set up guest instruction pointer */
	field = VMX_GUEST_RIP;
	if (get_vcpu_mode(vcpu) == REAL_MODE)
		value32 = 0;
	else
		value32 = (uint32_t) ((uint64_t) vcpu->entry_addr & 0xFFFFFFFF);

	pr_dbg("GUEST RIP on VMEntry %x ", value32);
	exec_vmwrite(field, value32);

	if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE) {
		/* Set up guest stack pointer to 0 */
		field = VMX_GUEST_RSP;
		value32 = 0;
		pr_dbg("GUEST RSP on VMEntry %x ",
				value32);
		exec_vmwrite(field, value32);
	}

	/***************************************************/
	/* Set up GDTR, IDTR and LDTR */
	/***************************************************/

	/* GDTR - Global Descriptor Table */
	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		/* Base */
		base = 0;

		/* Limit */
		limit = 0xFFFF;
	} else if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE) {
		uint64_t gdtb = 0;

		/* Base *//* TODO: Should guest GDTB point to host GDTB ? */
		/* Obtain the current global descriptor table base */
		asm volatile ("sgdt %0" : : "m" (gdtb));
		value32 = gdtb & 0x0ffff;
		gdtb = gdtb >> 16;	/* base */

		if ((gdtb >> 47 & 0x1))
			gdtb |= 0xffff000000000000ull;

		base = gdtb;

		/* Limit */
		limit = HOST_GDT_SIZE - 1;
	}

	/* GDTR Base */
	field = VMX_GUEST_GDTR_BASE;
	exec_vmwrite(field, base);
	pr_dbg("VMX_GUEST_GDTR_BASE: 0x%x ", base);

	/* GDTR Limit */
	field = VMX_GUEST_GDTR_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%x ", limit);

	/* IDTR - Interrupt Descriptor Table */
	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		/* Base */
		base = 0;

		/* Limit */
		limit = 0xFFFF;
	} else if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE) {
		uint64_t idtb = 0;

		/* TODO: Should guest IDTR point to host IDTR ? */
		asm volatile ("sidt %0"::"m" (idtb));
		value32 = idtb & 0x0ffff;
		/* Limit */
		limit = value32;
		idtb = idtb >> 16;	/* base */

		if ((idtb >> 47 & 0x1))
			idtb |= 0xffff000000000000ull;

		/* Base */
		base = idtb;
	}

	/* IDTR Base */
	field = VMX_GUEST_IDTR_BASE;
	exec_vmwrite(field, base);
	pr_dbg("VMX_GUEST_IDTR_BASE: 0x%x ", base);

	/* IDTR Limit */
	field = VMX_GUEST_IDTR_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%x ", limit);

	/***************************************************/
	/* Debug register */
	/***************************************************/
	/* Set up guest Debug register */
	field = VMX_GUEST_DR7;
	value = 0x400;
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_DR7: 0x%016llx ", value);

	/***************************************************/
	/* ES, CS, SS, DS, FS, GS */
	/***************************************************/
	data32_idx = 0x10;
	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		es = ss = ds = fs = gs = data32_idx;
		limit = 0xffff;

	} else if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE) {
		asm volatile ("movw %%es, %%ax":"=a" (es));
		asm volatile ("movw %%ss, %%ax":"=a" (ss));
		asm volatile ("movw %%ds, %%ax":"=a" (ds));
		asm volatile ("movw %%fs, %%ax":"=a" (fs));
		asm volatile ("movw %%gs, %%ax":"=a" (gs));
		limit = 0xffffffff;
	}

	/* Selector */
	field = VMX_GUEST_ES_SEL;
	exec_vmwrite(field, es);
	pr_dbg("VMX_GUEST_ES_SEL: 0x%x ", es);

	field = VMX_GUEST_SS_SEL;
	exec_vmwrite(field, ss);
	pr_dbg("VMX_GUEST_SS_SEL: 0x%x ", ss);

	field = VMX_GUEST_DS_SEL;
	exec_vmwrite(field, ds);
	pr_dbg("VMX_GUEST_DS_SEL: 0x%x ", ds);

	field = VMX_GUEST_FS_SEL;
	exec_vmwrite(field, fs);
	pr_dbg("VMX_GUEST_FS_SEL: 0x%x ", fs);

	field = VMX_GUEST_GS_SEL;
	exec_vmwrite(field, gs);
	pr_dbg("VMX_GUEST_GS_SEL: 0x%x ", gs);

	/* Limit */
	field = VMX_GUEST_ES_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_ES_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_SS_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_SS_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_DS_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_DS_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_FS_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_FS_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_GS_LIMIT;
	exec_vmwrite(field, limit);
	pr_dbg("VMX_GUEST_GS_LIMIT: 0x%x ", limit);

	/* Access */
	if (get_vcpu_mode(vcpu) == REAL_MODE)
		value32 = 0x0093;
	else if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE)
		value32 = 0xc093;

	field = VMX_GUEST_ES_ATTR;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_ES_ATTR: 0x%x ", value32);
	field = VMX_GUEST_SS_ATTR;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_SS_ATTR: 0x%x ", value32);
	field = VMX_GUEST_DS_ATTR;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_DS_ATTR: 0x%x ", value32);
	field = VMX_GUEST_FS_ATTR;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_FS_ATTR: 0x%x ", value32);
	field = VMX_GUEST_GS_ATTR;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_GS_ATTR: 0x%x ", value32);

	/* Base */
	value = 0;
	field = VMX_GUEST_ES_BASE;
	exec_vmwrite(field, es << 4);
	pr_dbg("VMX_GUEST_ES_BASE: 0x%016llx ", value);
	field = VMX_GUEST_SS_BASE;
	exec_vmwrite(field, ss << 4);
	pr_dbg("VMX_GUEST_SS_BASE: 0x%016llx ", value);
	field = VMX_GUEST_DS_BASE;
	exec_vmwrite(field, ds << 4);
	pr_dbg("VMX_GUEST_DS_BASE: 0x%016llx ", value);
	field = VMX_GUEST_FS_BASE;
	exec_vmwrite(field, fs << 4);
	pr_dbg("VMX_GUEST_FS_BASE: 0x%016llx ", value);
	field = VMX_GUEST_GS_BASE;
	exec_vmwrite(field, gs << 4);
	pr_dbg("VMX_GUEST_GS_BASE: 0x%016llx ", value);

	/***************************************************/
	/* LDT and TR (dummy) */
	/***************************************************/
	field = VMX_GUEST_LDTR_SEL;
	value32 = ldt_idx;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_LDTR_SEL: 0x%x ", value32);

	field = VMX_GUEST_LDTR_LIMIT;
	value32 = 0xffffffff;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_LDTR_LIMIT: 0x%x ", value32);

	field = VMX_GUEST_LDTR_ATTR;
	value32 = 0x10000;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_LDTR_ATTR: 0x%x ", value32);

	field = VMX_GUEST_LDTR_BASE;
	value32 = 0x00;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_LDTR_BASE: 0x%x ", value32);

	/* Task Register */
	field = VMX_GUEST_TR_SEL;
	value32 = lssd32_idx;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_TR_SEL: 0x%x ", value32);

	field = VMX_GUEST_TR_LIMIT;
	value32 = 0xff;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_TR_LIMIT: 0x%x ", value32);

	field = VMX_GUEST_TR_ATTR;
	value32 = 0x8b;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_TR_ATTR: 0x%x ", value32);

	field = VMX_GUEST_TR_BASE;
	value32 = 0x00;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_TR_BASE: 0x%x ", value32);

	field = VMX_GUEST_INTERRUPTIBILITY_INFO;
	value32 = 0;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_INTERRUPTIBILITY_INFO: 0x%x ",
		  value32);

	field = VMX_GUEST_ACTIVITY_STATE;
	value32 = 0;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_ACTIVITY_STATE: 0x%x ",
		  value32);

	field = VMX_GUEST_SMBASE;
	value32 = 0;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_SMBASE: 0x%x ", value32);

	asm volatile ("mov $0x174, %rcx");
	asm volatile ("rdmsr");
	asm volatile ("mov %%rax, %0"::"m" (value32):"memory");
	field = VMX_GUEST_IA32_SYSENTER_CS;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_GUEST_IA32_SYSENTER_CS: 0x%x ",
		  value32);

	value64 = PAT_POWER_ON_VALUE;
	exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value64);
	pr_dbg("VMX_GUEST_IA32_PAT: 0x%016llx ",
		  value64);

	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		/* Disable long mode (clear IA32_EFER.LME) in VMCS IA32_EFER
		 * MSR
		 */
		value64 = msr_read(MSR_IA32_EFER);
		value64 &= ~(MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT);
	} else {
		value64 = msr_read(MSR_IA32_EFER);
	}
	exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, value64);
	pr_dbg("VMX_GUEST_IA32_EFER: 0x%016llx ",
		  value64);

	value64 = 0;
	exec_vmwrite64(VMX_GUEST_IA32_DEBUGCTL_FULL, value64);
	pr_dbg("VMX_GUEST_IA32_DEBUGCTL: 0x%016llx ",
		  value64);

	/* Set up guest pending debug exception */
	field = VMX_GUEST_PENDING_DEBUG_EXCEPT;
	value = 0x0;
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_PENDING_DEBUG_EXCEPT: 0x%016llx ", value);

	/* These fields manage host and guest system calls * pg 3069 31.10.4.2
	 * - set up these fields with * contents of current SYSENTER ESP and
	 * EIP MSR values
	 */
	field = VMX_GUEST_IA32_SYSENTER_ESP;
	value = msr_read(MSR_IA32_SYSENTER_ESP);
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_IA32_SYSENTER_ESP: 0x%016llx ",
		  value);
	field = VMX_GUEST_IA32_SYSENTER_EIP;
	value = msr_read(MSR_IA32_SYSENTER_EIP);
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_IA32_SYSENTER_EIP: 0x%016llx ",
		  value);
}

static void init_host_state(__unused struct vcpu *vcpu)
{
	uint64_t field;
	uint16_t value16;
	uint32_t value32;
	uint64_t value64;
	uint64_t value;
	uint64_t trbase;
	uint64_t trbase_lo;
	uint64_t trbase_hi;
	uint64_t realtrbase;
	uint64_t gdtb = 0;
	uint64_t idtb = 0;
	uint16_t tr_sel;

	pr_dbg("*********************");
	pr_dbg("Initialize host state");
	pr_dbg("*********************");

	/***************************************************
	 * 16 - Bit fields
	 * Move the current ES, CS, SS, DS, FS, GS, TR, LDTR * values to the
	 * corresponding 16-bit host * segment selection (ES, CS, SS, DS, FS,
	 * GS), * Task Register (TR), * Local Descriptor Table Register (LDTR)
	 *
	 ***************************************************/
	field = VMX_HOST_ES_SEL;
	asm volatile ("movw %%es, %%ax":"=a" (value16));
	exec_vmwrite(field, value16);
	pr_dbg("VMX_HOST_ES_SEL: 0x%x ", value16);

	field = VMX_HOST_CS_SEL;
	asm volatile ("movw %%cs, %%ax":"=a" (value16));
	exec_vmwrite(field, value16);
	pr_dbg("VMX_HOST_CS_SEL: 0x%x ", value16);

	field = VMX_HOST_SS_SEL;
	asm volatile ("movw %%ss, %%ax":"=a" (value16));
	exec_vmwrite(field, value16);
	pr_dbg("VMX_HOST_SS_SEL: 0x%x ", value16);

	field = VMX_HOST_DS_SEL;
	asm volatile ("movw %%ds, %%ax":"=a" (value16));
	exec_vmwrite(field, value16);
	pr_dbg("VMX_HOST_DS_SEL: 0x%x ", value16);

	field = VMX_HOST_FS_SEL;
	asm volatile ("movw %%fs, %%ax":"=a" (value16));
	exec_vmwrite(field, value16);
	pr_dbg("VMX_HOST_FS_SEL: 0x%x ", value16);

	field = VMX_HOST_GS_SEL;
	asm volatile ("movw %%gs, %%ax":"=a" (value16));
	exec_vmwrite(field, value16);
	pr_dbg("VMX_HOST_GS_SEL: 0x%x ", value16);

	field = VMX_HOST_TR_SEL;
	asm volatile ("str %%ax":"=a" (tr_sel));
	exec_vmwrite(field, tr_sel);
	pr_dbg("VMX_HOST_TR_SEL: 0x%x ", tr_sel);

	/******************************************************
	 * 32-bit fields
	 * Set up the 32 bit host state fields - pg 3418 B.3.3 * Set limit for
	 * ES, CS, DD, DS, FS, GS, LDTR, Guest TR, * GDTR, and IDTR
	 ******************************************************/

	/* TODO: Should guest GDTB point to host GDTB ? */
	/* Obtain the current global descriptor table base */
	asm volatile ("sgdt %0"::"m" (gdtb));
	value32 = gdtb & 0x0ffff;
	gdtb = gdtb >> 16;	/* base */

	if ((gdtb >> 47) & 0x1)
		gdtb |= 0xffff000000000000ull;

	/* Set up the guest and host GDTB base fields with current GDTB base */
	field = VMX_HOST_GDTR_BASE;
	exec_vmwrite(field, gdtb);
	pr_dbg("VMX_HOST_GDTR_BASE: 0x%x ", gdtb);

	/* TODO: Should guest TR point to host TR ? */
	trbase = gdtb + tr_sel;
	if ((trbase >> 47) & 0x1)
		trbase |= 0xffff000000000000ull;

	/* SS segment override */
	asm volatile ("mov %0,%%rax\n"
		      ".byte 0x36\n"
		      "movq (%%rax),%%rax\n":"=a" (trbase_lo):"0"(trbase)
	    );
	realtrbase = ((trbase_lo >> 16) & (0x0ffff)) |
	    (((trbase_lo >> 32) & 0x000000ff) << 16) |
	    (((trbase_lo >> 56) & 0xff) << 24);

	/* SS segment override for upper32 bits of base in ia32e mode */
	asm volatile ("mov %0,%%rax\n"
		      ".byte 0x36\n"
		      "movq 8(%%rax),%%rax\n":"=a" (trbase_hi):"0"(trbase));
	realtrbase = realtrbase | (trbase_hi << 32);

	/* Set up host and guest TR base fields */
	field = VMX_HOST_TR_BASE;
	exec_vmwrite(field, realtrbase);
	pr_dbg("VMX_HOST_TR_BASE: 0x%x ", realtrbase);

	/* Obtain the current interrupt descriptor table base */
	asm volatile ("sidt %0"::"m" (idtb));
	value32 = idtb & 0x0ffff;
	/* base */
	idtb = idtb >> 16;

	if ((idtb >> 47 & 0x1))
		idtb |= 0xffff000000000000ull;

	field = VMX_HOST_IDTR_BASE;
	exec_vmwrite(field, idtb);
	pr_dbg("VMX_HOST_IDTR_BASE: 0x%x ", idtb);

	asm volatile ("mov $0x174, %rcx");
	asm volatile ("rdmsr");
	asm volatile ("mov %%rax, %0"::"m" (value32):"memory");
	field = VMX_HOST_IA32_SYSENTER_CS;
	exec_vmwrite(field, value32);
	pr_dbg("VMX_HOST_IA32_SYSENTER_CS: 0x%x ",
			value32);

	/**************************************************/
	/* 64-bit fields */
	pr_dbg("64-bit********");

	value64 = msr_read(MSR_IA32_PAT);
	exec_vmwrite64(VMX_HOST_IA32_PAT_FULL, value64);
	pr_dbg("VMX_HOST_IA32_PAT: 0x%016llx ", value64);

	value64 = msr_read(MSR_IA32_EFER);
	exec_vmwrite64(VMX_HOST_IA32_EFER_FULL, value64);
	pr_dbg("VMX_HOST_IA32_EFER: 0x%016llx ",
			value64);

	/**************************************************/
	/* Natural width fields */
	pr_dbg("Natural-width********");
	/* Set up host CR0 field */
	CPU_CR_READ(cr0, &value);
	value = (uint32_t) value;
	field = VMX_HOST_CR0;
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_CR0: 0x%016llx ", value);

	/* Set up host CR3 field */
	CPU_CR_READ(cr3, &value);
	value = (uint32_t) value;
	field = VMX_HOST_CR3;
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_CR3: 0x%016llx ", value);

	/* Set up host CR4 field */
	CPU_CR_READ(cr4, &value);
	value = (uint32_t) value;
	field = VMX_HOST_CR4;
	exec_vmwrite(field, value);
	pr_dbg("VMX_GUEST_CR4: 0x%016llx ", value);

	/* Set up host and guest FS base address */
	value = msr_read(MSR_IA32_FS_BASE);
	field = VMX_HOST_FS_BASE;
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_FS_BASE: 0x%016llx ", value);
	value = msr_read(MSR_IA32_GS_BASE);
	field = VMX_HOST_GS_BASE;
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_GS_BASE: 0x%016llx ", value);

	/* Set up host instruction pointer on VM Exit */
	field = VMX_HOST_RIP;
	value32 = (uint32_t) ((uint64_t) (&vm_exit) & 0xFFFFFFFF);
	pr_dbg("HOST RIP on VMExit %x ", value32);
	exec_vmwrite(field, value32);
	pr_dbg("vm exit return address = %x ", value32);

	/* These fields manage host and guest system calls * pg 3069 31.10.4.2
	 * - set up these fields with * contents of current SYSENTER ESP and
	 * EIP MSR values
	 */
	field = VMX_HOST_IA32_SYSENTER_ESP;
	value = msr_read(MSR_IA32_SYSENTER_ESP);
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_IA32_SYSENTER_ESP: 0x%016llx ",
		  value);
	field = VMX_HOST_IA32_SYSENTER_EIP;
	value = msr_read(MSR_IA32_SYSENTER_EIP);
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_IA32_SYSENTER_EIP: 0x%016llx ", value);
}

static void init_exec_ctrl(struct vcpu *vcpu)
{
	uint32_t value32, fixed0, fixed1;
	uint64_t value64;
	struct vm *vm = (struct vm *) vcpu->vm;

	/* Log messages to show initializing VMX execution controls */
	pr_dbg("*****************************");
	pr_dbg("Initialize execution control ");
	pr_dbg("*****************************");

	/* Set up VM Execution control to enable Set VM-exits on external
	 * interrupts preemption timer - pg 2899 24.6.1
	 */
	value32 = msr_read(MSR_IA32_VMX_PINBASED_CTLS);


	/* enable external interrupt VM Exit */
	value32 |= VMX_PINBASED_CTLS_IRQ_EXIT;

	exec_vmwrite(VMX_PIN_VM_EXEC_CONTROLS, value32);
	pr_dbg("VMX_PIN_VM_EXEC_CONTROLS: 0x%x ", value32);

	/* Set up primary processor based VM execution controls - pg 2900
	 * 24.6.2. Set up for:
	 * Enable TSC offsetting
	 * Enable TSC exiting
	 * guest access to IO bit-mapped ports causes VM exit
	 * guest access to MSR causes VM exit
	 * Activate secondary controls
	 */
	/* These are bits 1,4-6,8,13-16, and 26, the corresponding bits of
	 * the IA32_VMX_PROCBASED_CTRLS MSR are always read as 1 --- A.3.2
	 */
	value32 = msr_read(MSR_IA32_VMX_PROCBASED_CTLS);
	value32 |= (VMX_PROCBASED_CTLS_TSC_OFF |
		    /* VMX_PROCBASED_CTLS_RDTSC | */
		    VMX_PROCBASED_CTLS_IO_BITMAP |
		    VMX_PROCBASED_CTLS_MSR_BITMAP |
		    VMX_PROCBASED_CTLS_SECONDARY);

	/*Disable VM_EXIT for CR3 access*/
	value32 &= ~(VMX_PROCBASED_CTLS_CR3_LOAD |
			VMX_PROCBASED_CTLS_CR3_STORE);

	if (is_vapic_supported()) {
		value32 |= VMX_PROCBASED_CTLS_TPR_SHADOW;
	} else {
		/* Add CR8 VMExit for vlapic */
		value32 |=
			(VMX_PROCBASED_CTLS_CR8_LOAD |
			VMX_PROCBASED_CTLS_CR8_STORE);
	}

	exec_vmwrite(VMX_PROC_VM_EXEC_CONTROLS, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS: 0x%x ", value32);

	/* Set up secondary processor based VM execution controls - pg 2901
	 * 24.6.2. Set up for: * Enable EPT * Enable RDTSCP * Unrestricted
	 * guest (optional)
	 */
	value32 = msr_read(MSR_IA32_VMX_PROCBASED_CTLS2);
	value32 |= (VMX_PROCBASED_CTLS2_EPT |
		    /* VMX_PROCBASED_CTLS2_RDTSCP | */
		    VMX_PROCBASED_CTLS2_UNRESTRICT);

	if (is_vapic_supported()) {
		value32 |= VMX_PROCBASED_CTLS2_VAPIC;

		if (is_vapic_virt_reg_supported())
			value32 |= VMX_PROCBASED_CTLS2_VAPIC_REGS;

		if (is_vapic_intr_delivery_supported())
			value32 |= VMX_PROCBASED_CTLS2_VIRQ;
		else
			/*
			 * This field exists only on processors that support
			 * the 1-setting  of the "use TPR shadow"
			 * VM-execution control.
			 *
			 * Set up TPR threshold for virtual interrupt delivery
			 * - pg 2904 24.6.8
			 */
			exec_vmwrite(VMX_TPR_THRESHOLD, 0);
	}

	if (is_xsave_supported()) {
		exec_vmwrite64(VMX_XSS_EXITING_BITMAP_FULL, 0);
		value32 |= VMX_PROCBASED_CTLS2_XSVE_XRSTR;
	}

	exec_vmwrite(VMX_PROC_VM_EXEC_CONTROLS2, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS2: 0x%x ", value32);

	if (is_vapic_supported()) {
		/*APIC-v, config APIC-access address*/
		value64 = apicv_get_apic_access_addr(vcpu->vm);
		exec_vmwrite64(VMX_APIC_ACCESS_ADDR_FULL,
						value64);

		/*APIC-v, config APIC virtualized page address*/
		value64 = apicv_get_apic_page_addr(vcpu->arch_vcpu.vlapic);
		exec_vmwrite64(VMX_VIRTUAL_APIC_PAGE_ADDR_FULL,
						value64);

		if (is_vapic_intr_delivery_supported()) {
			/* these fields are supported only on processors
			 * that support the 1-setting of the "virtual-interrupt
			 * delivery" VM-execution control
			 */
			exec_vmwrite64(VMX_EOI_EXIT0_FULL, -1UL);
			exec_vmwrite64(VMX_EOI_EXIT1_FULL, -1UL);
			exec_vmwrite64(VMX_EOI_EXIT2_FULL, -1UL);
			exec_vmwrite64(VMX_EOI_EXIT3_FULL, -1UL);
		}
	}

	/* Check for EPT support */
	if (is_ept_supported())
		pr_dbg("EPT is supported");
	else
		pr_err("Error: EPT is not supported");

	/* Load EPTP execution control
	 * TODO: introduce API to make this data driven based
	 * on VMX_EPT_VPID_CAP
	 */
	value64 = vm->arch_vm.nworld_eptp | (3 << 3) | 6;
	exec_vmwrite64(VMX_EPT_POINTER_FULL, value64);
	pr_dbg("VMX_EPT_POINTER: 0x%016llx ", value64);

	/* Set up guest exception mask bitmap setting a bit * causes a VM exit
	 * on corresponding guest * exception - pg 2902 24.6.3
	 * enable VM exit on MC and DB
	 */
	value32 = (1 << IDT_MC) | (1u << IDT_DB);
	exec_vmwrite(VMX_EXCEPTION_BITMAP, value32);

	/* Set up page fault error code mask - second paragraph * pg 2902
	 * 24.6.3 - guest page fault exception causing * vmexit is governed by
	 * both VMX_EXCEPTION_BITMAP and * VMX_PF_EC_MASK
	 */
	exec_vmwrite(VMX_PF_EC_MASK, 0);

	/* Set up page fault error code match - second paragraph * pg 2902
	 * 24.6.3 - guest page fault exception causing * vmexit is governed by
	 * both VMX_EXCEPTION_BITMAP and * VMX_PF_EC_MATCH
	 */
	exec_vmwrite(VMX_PF_EC_MATCH, 0);

	/* Set up CR3 target count - An execution of mov to CR3 * by guest
	 * causes HW to evaluate operand match with * one of N CR3-Target Value
	 * registers. The CR3 target * count values tells the number of
	 * target-value regs to evaluate
	 */
	exec_vmwrite(VMX_CR3_TARGET_COUNT, 0);

	/* Set up IO bitmap register A and B - pg 2902 24.6.4 */
	value64 = (int64_t) vm->arch_vm.iobitmap[0];
	exec_vmwrite64(VMX_IO_BITMAP_A_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_A: 0x%016llx ", value64);
	value64 = (int64_t) vm->arch_vm.iobitmap[1];
	exec_vmwrite64(VMX_IO_BITMAP_B_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_B: 0x%016llx ", value64);

	init_msr_emulation(vcpu);

	/* Set up executive VMCS pointer - pg 2905 24.6.10 */
	exec_vmwrite64(VMX_EXECUTIVE_VMCS_PTR_FULL, 0);

	/* Setup Time stamp counter offset - pg 2902 24.6.5 */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, 0);

	/* Set up the link pointer */
	exec_vmwrite64(VMX_VMS_LINK_PTR_FULL, 0xFFFFFFFFFFFFFFFF);

	/* Natural-width */
	pr_dbg("Natural-width*********");

	/* Read the CR0 fixed0 / fixed1 MSR registers */
	fixed0 = msr_read(MSR_IA32_VMX_CR0_FIXED0);
	fixed1 = msr_read(MSR_IA32_VMX_CR0_FIXED1);

	if (get_vcpu_mode(vcpu) == REAL_MODE) {
		/* Check to see if unrestricted guest support is available */
		if (msr_read(MSR_IA32_VMX_MISC) & (1 << 5)) {
			/* Adjust fixed bits as they can/will reflect incorrect
			 * settings that ARE valid in unrestricted guest mode.
			 * Both PG and PE bits can bit changed in unrestricted
			 * guest mode.
			 */
			fixed0 &= ~(CR0_PG | CR0_PE);
			fixed1 |= (CR0_PG | CR0_PE);

			/* Log success for unrestricted mode being present */
			pr_dbg("Unrestricted support is available. ");
		} else {
			/* Log failure for unrestricted mode NOT being
			 * present
			 */
			pr_err("Error: Unrestricted support is not available");
			/* IA32_VMX_MISC bit 5 clear */
		}

	}

	/* (get_vcpu_mode(vcpu) == REAL_MODE) */
	/* Output fixed CR0 values */
	pr_dbg("Fixed0 CR0 value: 0x%x", fixed0);
	pr_dbg("Fixed1 CR0 value: 0x%x", fixed1);

	/* Determine which bits are "flexible" in CR0 - allowed to be changed
	 * as per arch manual in VMX operation.  Any bits that are different
	 * between fixed0 and fixed1 are "flexible" and the guest can change.
	 */
	value32 = fixed0 ^ fixed1;

	/* Set the CR0 mask to the inverse of the "flexible" bits */
	value32 = ~value32;
	exec_vmwrite(VMX_CR0_MASK, value32);

	/* Output CR0 mask value */
	pr_dbg("CR0 mask value: 0x%x", value32);

	/* Calculate the CR0 shadow register value that will be used to enforce
	 * the correct values for host owned bits
	 */
	value32 = (fixed0 | fixed1) & value32;
	exec_vmwrite(VMX_CR0_READ_SHADOW, value32);

	/* Output CR0 shadow value */
	pr_dbg("CR0 shadow value: 0x%x", value32);

	/* Read the CR4 fixed0 / fixed1 MSR registers */
	fixed0 = msr_read(MSR_IA32_VMX_CR4_FIXED0);
	fixed1 = msr_read(MSR_IA32_VMX_CR4_FIXED1);

	/* Output fixed CR0 values */
	pr_dbg("Fixed0 CR4 value: 0x%x", fixed0);
	pr_dbg("Fixed1 CR4 value: 0x%x", fixed1);

	/* Determine which bits are "flexible" in CR4 - allowed to be changed
	 * as per arch manual in VMX operation.  Any bits that are different
	 * between fixed0 and fixed1 are "flexible" and the guest can change.
	 */
	value32 = fixed0 ^ fixed1;

	/* Set the CR4 mask to the inverse of the "flexible" bits */
	value32 = ~value32;
	exec_vmwrite(VMX_CR4_MASK, value32);

	/* Output CR4 mask value */
	pr_dbg("CR4 mask value: 0x%x", value32);

	/* Calculate the CR4 shadow register value that will be used to enforce
	 * the correct values for host owned bits
	 */
	value32 = (fixed0 | fixed1) & value32;
	exec_vmwrite(VMX_CR4_READ_SHADOW, value32);

	/* Output CR4 shadow value */
	pr_dbg("CR4 shadow value: 0x%x", value32);

	/* The CR3 target registers work in concert with VMX_CR3_TARGET_COUNT
	 * field. Using these registers guest CR3 access can be managed. i.e.,
	 * if operand does not match one of these register values a VM exit
	 * would occur
	 */
	exec_vmwrite(VMX_CR3_TARGET_0, 0);
	exec_vmwrite(VMX_CR3_TARGET_1, 0);
	exec_vmwrite(VMX_CR3_TARGET_2, 0);
	exec_vmwrite(VMX_CR3_TARGET_3, 0);
}

static void init_entry_ctrl(__unused struct vcpu *vcpu)
{
	uint32_t value32;

	/* Log messages to show initializing VMX entry controls */
	pr_dbg("*************************");
	pr_dbg("Initialize Entry control ");
	pr_dbg("*************************");

	/* Set up VMX entry controls - pg 2908 24.8.1 * Set IA32e guest mode -
	 * on VM entry processor is in IA32e 64 bitmode * Start guest with host
	 * IA32_PAT and IA32_EFER
	 */
	value32 = msr_read(MSR_IA32_VMX_ENTRY_CTLS);
	if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE)
		value32 |= (VMX_ENTRY_CTLS_IA32E_MODE);

	value32 |= (VMX_ENTRY_CTLS_LOAD_EFER |
		    VMX_ENTRY_CTLS_LOAD_PAT);

	exec_vmwrite(VMX_ENTRY_CONTROLS, value32);
	pr_dbg("VMX_ENTRY_CONTROLS: 0x%x ", value32);

	/* Set up VMX entry MSR load count - pg 2908 24.8.2 Tells the number of
	 * MSRs on load from memory on VM entry from mem address provided by
	 * VM-entry MSR load address field
	 */
	exec_vmwrite(VMX_ENTRY_MSR_LOAD_COUNT, 0);

	/* Set up VM entry interrupt information field pg 2909 24.8.3 */
	exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD, 0);

	/* Set up VM entry exception error code - pg 2910 24.8.3 */
	exec_vmwrite(VMX_ENTRY_EXCEPTION_EC, 0);

	/* Set up VM entry instruction length - pg 2910 24.8.3 */
	exec_vmwrite(VMX_ENTRY_INSTR_LENGTH, 0);
}

static void init_exit_ctrl(__unused struct vcpu *vcpu)
{
	uint32_t value32;

	/* Log messages to show initializing VMX entry controls */
	pr_dbg("************************");
	pr_dbg("Initialize Exit control ");
	pr_dbg("************************");

	/* Set up VM exit controls - pg 2907 24.7.1 for: Host address space
	 * size is 64 bit Set up to acknowledge interrupt on exit, if 1 the HW
	 * acks the interrupt in VMX non-root and saves the interrupt vector to
	 * the relevant VM exit field for further processing by Hypervisor
	 * Enable saving and loading of IA32_PAT and IA32_EFER on VMEXIT Enable
	 * saving of pre-emption timer on VMEXIT
	 */
	value32 = msr_read(MSR_IA32_VMX_EXIT_CTLS);
	value32 |= (VMX_EXIT_CTLS_ACK_IRQ |
		    VMX_EXIT_CTLS_SAVE_PAT |
		    VMX_EXIT_CTLS_LOAD_PAT |
		    VMX_EXIT_CTLS_LOAD_EFER |
		    VMX_EXIT_CTLS_SAVE_EFER |
		    VMX_EXIT_CTLS_HOST_ADDR64);

	exec_vmwrite(VMX_EXIT_CONTROLS, value32);
	pr_dbg("VMX_EXIT_CONTROL: 0x%x ", value32);

	/* Set up VM exit MSR store and load counts pg 2908 24.7.2 - tells the
	 * HW number of MSRs to stored to mem and loaded from mem on VM exit.
	 * The 64 bit VM-exit MSR store and load address fields provide the
	 * corresponding addresses
	 */
	exec_vmwrite(VMX_EXIT_MSR_STORE_COUNT, 0);
	exec_vmwrite(VMX_EXIT_MSR_LOAD_COUNT, 0);
}

#ifdef CONFIG_EFI_STUB
static void override_uefi_vmcs(struct vcpu *vcpu)
{
	uint64_t field;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	if (get_vcpu_mode(vcpu) == PAGE_PROTECTED_MODE) {
		/* Set up guest CR0 field */
		field = VMX_GUEST_CR0;
		cur_context->cr0 = efi_ctx->cr0 | CR0_PG | CR0_PE | CR0_NE;
		exec_vmwrite(field, cur_context->cr0 & 0xFFFFFFFF);
		pr_dbg("VMX_GUEST_CR0: 0x%016llx ", cur_context->cr0);

		/* Set up guest CR3 field */
		field = VMX_GUEST_CR3;
		cur_context->cr3 = efi_ctx->cr3;
		exec_vmwrite(field, cur_context->cr3 & 0xFFFFFFFF);
		pr_dbg("VMX_GUEST_CR3: 0x%016llx ", cur_context->cr3);

		/* Set up guest CR4 field */
		field = VMX_GUEST_CR4;
		cur_context->cr4 = efi_ctx->cr4 | CR4_VMXE;
		exec_vmwrite(field, cur_context->cr4 & 0xFFFFFFFF);
		pr_dbg("VMX_GUEST_CR4: 0x%016llx ", cur_context->cr4);

		/* Selector */
		field = VMX_GUEST_CS_SEL;
		exec_vmwrite(field, efi_ctx->cs_sel);
		pr_dbg("VMX_GUEST_CS_SEL: 0x%x ", efi_ctx->cs_sel);

		/* Access */
		field = VMX_GUEST_CS_ATTR;
		exec_vmwrite(field, efi_ctx->cs_ar);
		pr_dbg("VMX_GUEST_CS_ATTR: 0x%x ", efi_ctx->cs_ar);

		field = VMX_GUEST_ES_SEL;
		exec_vmwrite(field, efi_ctx->es_sel);
		pr_dbg("VMX_GUEST_ES_SEL: 0x%x ", efi_ctx->es_sel);

		field = VMX_GUEST_SS_SEL;
		exec_vmwrite(field, efi_ctx->ss_sel);
		pr_dbg("VMX_GUEST_SS_SEL: 0x%x ", efi_ctx->ss_sel);

		field = VMX_GUEST_DS_SEL;
		exec_vmwrite(field, efi_ctx->ds_sel);
		pr_dbg("VMX_GUEST_DS_SEL: 0x%x ", efi_ctx->ds_sel);

		field = VMX_GUEST_FS_SEL;
		exec_vmwrite(field, efi_ctx->fs_sel);
		pr_dbg("VMX_GUEST_FS_SEL: 0x%x ", efi_ctx->fs_sel);

		field = VMX_GUEST_GS_SEL;
		exec_vmwrite(field, efi_ctx->gs_sel);
		pr_dbg("VMX_GUEST_GS_SEL: 0x%x ", efi_ctx->gs_sel);

		/* Base */
		field = VMX_GUEST_ES_BASE;
		exec_vmwrite(field, efi_ctx->es_sel << 4);
		field = VMX_GUEST_SS_BASE;
		exec_vmwrite(field, efi_ctx->ss_sel << 4);
		field = VMX_GUEST_DS_BASE;
		exec_vmwrite(field, efi_ctx->ds_sel << 4);
		field = VMX_GUEST_FS_BASE;
		exec_vmwrite(field, efi_ctx->fs_sel << 4);
		field = VMX_GUEST_GS_BASE;
		exec_vmwrite(field, efi_ctx->gs_sel << 4);

		/* RSP */
		field = VMX_GUEST_RSP;
		exec_vmwrite(field, efi_ctx->rsp);
		pr_dbg("GUEST RSP on VMEntry %x ", efi_ctx->rsp);

		/* GDTR Base */
		field = VMX_GUEST_GDTR_BASE;
		exec_vmwrite(field, (uint64_t)efi_ctx->gdt.base);
		pr_dbg("VMX_GUEST_GDTR_BASE: 0x%x ", efi_ctx->gdt.base);

		/* GDTR Limit */
		field = VMX_GUEST_GDTR_LIMIT;
		exec_vmwrite(field, efi_ctx->gdt.limit);
		pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%x ", efi_ctx->gdt.limit);

		/* IDTR Base */
		field = VMX_GUEST_IDTR_BASE;
		exec_vmwrite(field, (uint64_t)efi_ctx->idt.base);
		pr_dbg("VMX_GUEST_IDTR_BASE: 0x%x ", efi_ctx->idt.base);

		/* IDTR Limit */
		field = VMX_GUEST_IDTR_LIMIT;
		exec_vmwrite(field, efi_ctx->idt.limit);
		pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%x ", efi_ctx->idt.limit);
	}

	/* Interrupt */
	field = VMX_GUEST_RFLAGS;
	/* clear flags for CF/PF/AF/ZF/SF/OF */
	cur_context->rflags = efi_ctx->rflags & ~(0x8d5);
	exec_vmwrite(field, cur_context->rflags);
	pr_dbg("VMX_GUEST_RFLAGS: 0x%016llx ", cur_context->rflags);
}
#endif

int init_vmcs(struct vcpu *vcpu)
{
	uint32_t vmx_rev_id;
	int status = 0;

	if (vcpu == NULL)
		status = -EINVAL;
	ASSERT(status == 0, "Incorrect arguments");

	/* Log message */
	pr_dbg("Initializing VMCS");

	/* Obtain the VM Rev ID from HW and populate VMCS page with it */
	vmx_rev_id = msr_read(MSR_IA32_VMX_BASIC);
	memcpy_s((void *) vcpu->arch_vcpu.vmcs, 4, &vmx_rev_id, 4);

	/* Execute VMCLEAR on current VMCS */
	status = exec_vmclear((void *)&vcpu->arch_vcpu.vmcs);
	ASSERT(status == 0, "Failed VMCLEAR during VMCS setup!");

	/* Load VMCS pointer */
	status = exec_vmptrld((void *)&vcpu->arch_vcpu.vmcs);
	ASSERT(status == 0, "Failed VMCS pointer load!");

	/* Initialize the Virtual Machine Control Structure (VMCS) */
	init_host_state(vcpu);
	init_guest_state(vcpu);
	init_exec_ctrl(vcpu);
	init_entry_ctrl(vcpu);
	init_exit_ctrl(vcpu);

#ifdef CONFIG_EFI_STUB
	if (is_vm0(vcpu->vm) && vcpu->pcpu_id == 0)
		override_uefi_vmcs(vcpu);
#endif
	/* Return status to caller */
	return status;
}
