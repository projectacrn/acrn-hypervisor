/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <vm0_boot.h>
#include <cpu.h>
#ifdef CONFIG_EFI_STUB
extern struct efi_context* efi_ctx;
#endif

#define REAL_MODE_BSP_INIT_CODE_SEL	(0xf000U)
#define REAL_MODE_DATA_SEG_AR		(0x0093U)
#define REAL_MODE_CODE_SEG_AR		(0x009fU)
#define PROTECTED_MODE_DATA_SEG_AR	(0xc093U)
#define PROTECTED_MODE_CODE_SEG_AR	(0xc09bU)
#define DR7_INIT_VALUE			(0x400UL)
#define LDTR_AR				(0x0082U) /* LDT, type must be 2, refer to SDM Vol3 26.3.1.2 */
#define TR_AR				(0x008bU) /* TSS (busy), refer to SDM Vol3 26.3.1.2 */

static uint64_t cr0_host_mask;
static uint64_t cr0_always_on_mask;
static uint64_t cr0_always_off_mask;
static uint64_t cr4_host_mask;
static uint64_t cr4_always_on_mask;
static uint64_t cr4_always_off_mask;

void update_msr_bitmap_x2apic_apicv(struct acrn_vcpu *vcpu);
void update_msr_bitmap_x2apic_passthru(struct acrn_vcpu *vcpu);

bool is_vmx_disabled(void)
{
	uint64_t msr_val;

	/* Read Feature ControL MSR */
	msr_val = msr_read(MSR_IA32_FEATURE_CONTROL);

	/* Check if feature control is locked and vmx cannot be enabled */
	if ((msr_val & MSR_IA32_FEATURE_CONTROL_LOCK) != 0U &&
		(msr_val & MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX) == 0U) {
		return true;
	}
	return false;
}

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
 */
void exec_vmxon_instr(uint16_t pcpu_id)
{
	uint64_t tmp64, vmcs_pa;
	uint32_t tmp32;
	void *vmxon_region_va = (void *)per_cpu(vmxon_region, pcpu_id);
	uint64_t vmxon_region_pa;
	struct acrn_vcpu *vcpu = get_ever_run_vcpu(pcpu_id);

	/* Initialize vmxon page with revision id from IA32 VMX BASIC MSR */
	tmp32 = (uint32_t)msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s((uint32_t *) vmxon_region_va, 4U, (void *)&tmp32, 4U);

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

	vmcs_pa = hva2hpa(vcpu->arch.vmcs);
	exec_vmptrld(&vmcs_pa);
}

static inline void exec_vmxoff(void)
{
	asm volatile ("vmxoff" : : : "memory");
}

void vmx_off(uint16_t pcpu_id)
{

	struct acrn_vcpu *vcpu = get_ever_run_vcpu(pcpu_id);
	uint64_t vmcs_pa;

	vmcs_pa = hva2hpa(vcpu->arch.vmcs);
	exec_vmclear((void *)&vmcs_pa);

	exec_vmxoff();
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

static void init_cr0_cr4_host_mask(void)
{
	static bool inited = false;
	uint64_t fixed0, fixed1;
	if (!inited) {
		/* Read the CR0 fixed0 / fixed1 MSR registers */
		fixed0 = msr_read(MSR_IA32_VMX_CR0_FIXED0);
		fixed1 = msr_read(MSR_IA32_VMX_CR0_FIXED1);

		cr0_host_mask = ~(fixed0 ^ fixed1);
		/* Add the bit hv wants to trap */
		cr0_host_mask |= CR0_TRAP_MASK;
		/* CR0 clear PE/PG from always on bits due to "unrestructed
		 * guest" feature */
		cr0_always_on_mask = fixed0 & (~(CR0_PE | CR0_PG));
		cr0_always_off_mask = ~fixed1;
		/* SDM 2.5
		 * bit 63:32 of CR0 and CR4 ar reserved and must be written
		 * zero. We could merge it with always off mask.
		 */
		cr0_always_off_mask |= 0xFFFFFFFF00000000UL;

		/* Read the CR4 fixed0 / fixed1 MSR registers */
		fixed0 = msr_read(MSR_IA32_VMX_CR4_FIXED0);
		fixed1 = msr_read(MSR_IA32_VMX_CR4_FIXED1);

		cr4_host_mask = ~(fixed0 ^ fixed1);
		/* Add the bit hv wants to trap */
		cr4_host_mask |= CR4_TRAP_MASK;
		cr4_always_on_mask = fixed0;
		/* Record the bit fixed to 0 for CR4, including reserved bits */
		cr4_always_off_mask = ~fixed1;
		/* SDM 2.5
		 * bit 63:32 of CR0 and CR4 ar reserved and must be written
		 * zero. We could merge it with always off mask.
		 */
		cr4_always_off_mask |= 0xFFFFFFFF00000000UL;
		cr4_always_off_mask |= CR4_RESERVED_MASK;
		inited = true;
	}

	exec_vmwrite(VMX_CR0_MASK, cr0_host_mask);
	/* Output CR0 mask value */
	pr_dbg("CR0 mask value: 0x%016llx", cr0_host_mask);


	exec_vmwrite(VMX_CR4_MASK, cr4_host_mask);
	/* Output CR4 mask value */
	pr_dbg("CR4 mask value: 0x%016llx", cr4_host_mask);
}

uint64_t vmx_rdmsr_pat(const struct acrn_vcpu *vcpu)
{
	/*
	 * note: if context->cr0.CD is set, the actual value in guest's
	 * IA32_PAT MSR is PAT_ALL_UC_VALUE, which may be different from
	 * the saved value saved_context->ia32_pat
	 */
	return vcpu_get_pat_ext(vcpu);
}

int vmx_wrmsr_pat(struct acrn_vcpu *vcpu, uint64_t value)
{
	uint32_t i;
	uint64_t field;

	for (i = 0U; i < 8U; i++) {
		field = (value >> (i * 8U)) & 0xffUL;
		if (pat_mem_type_invalid(field) ||
				((PAT_FIELD_RSV_BITS & field) != 0UL)) {
			pr_err("invalid guest IA32_PAT: 0x%016llx", value);
			return -EINVAL;
		}
	}

	vcpu_set_pat_ext(vcpu, value);

	/*
	 * If context->cr0.CD is set, we defer any further requests to write
	 * guest's IA32_PAT, until the time when guest's CR0.CD is being cleared
	 */
	if ((vcpu_get_cr0(vcpu) & CR0_CD) == 0UL) {
		exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value);
	}

	return 0;
}

static void load_pdptrs(const struct acrn_vcpu *vcpu)
{
	uint64_t guest_cr3 = exec_vmread(VMX_GUEST_CR3);
	/* TODO: check whether guest cr3 is valid */
	uint64_t *guest_cr3_hva = (uint64_t *)gpa2hva(vcpu->vm, guest_cr3);

	exec_vmwrite64(VMX_GUEST_PDPTE0_FULL, get_pgentry(guest_cr3_hva + 0UL));
	exec_vmwrite64(VMX_GUEST_PDPTE1_FULL, get_pgentry(guest_cr3_hva + 1UL));
	exec_vmwrite64(VMX_GUEST_PDPTE2_FULL, get_pgentry(guest_cr3_hva + 2UL));
	exec_vmwrite64(VMX_GUEST_PDPTE3_FULL, get_pgentry(guest_cr3_hva + 3UL));
}

static bool is_cr0_write_valid(struct acrn_vcpu *vcpu, uint64_t cr0)
{
	/* Shouldn't set always off bit */
	if ((cr0 & cr0_always_off_mask) != 0UL) {
		return false;
	}

	/* SDM 25.3 "Changes to instruction behavior in VMX non-root"
	 *
	 * We always require "unrestricted guest" control enabled. So
	 *
	 * CR0.PG = 1, CR4.PAE = 0 and IA32_EFER.LME = 1 is invalid.
	 * CR0.PE = 0 and CR0.PG = 1 is invalid.
	 */
	if (((cr0 & CR0_PG) != 0UL) && !is_pae(vcpu)
		&& ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL)) {
		return false;
	}

	if (((cr0 & CR0_PE) == 0UL) && ((cr0 & CR0_PG) != 0UL)) {
		return false;
	}

	/* SDM 6.15 "Exception and Interrupt Refrerence" GP Exception
	 *
	 * Loading CR0 regsiter with a set NW flag and a clear CD flag
	 * is invalid
	 */
	if (((cr0 & CR0_CD) == 0UL) && ((cr0 & CR0_NW) != 0UL)) {
		return false;
	}

	return true;
}

/*
 * Handling of CR0:
 * Assume "unrestricted guest" feature is supported by vmx.
 * For mode switch, hv only needs to take care of enabling/disabling long mode,
 * thanks to "unrestricted guest" feature.
 *
 *   - PE (0)  Trapped to track cpu mode.
 *             Set the value according to the value from guest.
 *   - MP (1)  Flexible to guest
 *   - EM (2)  Flexible to guest
 *   - TS (3)  Flexible to guest
 *   - ET (4)  Flexible to guest
 *   - NE (5)  must always be 1
 *   - WP (16) Trapped to get if it inhibits supervisor level procedures to
 *             write into ro-pages.
 *   - AM (18) Flexible to guest
 *   - NW (29) Trapped to emulate cache disable situation
 *   - CD (30) Trapped to emulate cache disable situation
 *   - PG (31) Trapped to track cpu/paging mode.
 *             Set the value according to the value from guest.
 */
void vmx_write_cr0(struct acrn_vcpu *vcpu, uint64_t cr0)
{
	uint64_t cr0_vmx;
	uint32_t entry_ctrls;
	bool old_paging_enabled = is_paging_enabled(vcpu);
	uint64_t cr0_changed_bits = vcpu_get_cr0(vcpu) ^ cr0;

	if (!is_cr0_write_valid(vcpu, cr0)) {
		pr_dbg("Invalid cr0 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
		return;
	}

	/* SDM 2.5
	 * When loading a control register, reserved bit should always set
	 * to the value previously read.
	 */
	cr0 &= ~CR0_RESERVED_MASK;

	if (!old_paging_enabled && ((cr0 & CR0_PG) != 0UL)) {
		if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
			/* Enable long mode */
			pr_dbg("VMM: Enable long mode");
			entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
			entry_ctrls |= VMX_ENTRY_CTLS_IA32E_MODE;
			exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

			vcpu_set_efer(vcpu,
				vcpu_get_efer(vcpu) | MSR_IA32_EFER_LMA_BIT);
		} else if (is_pae(vcpu)) {
			/* enabled PAE from paging disabled */
			load_pdptrs(vcpu);
		} else {
		}
	} else if (old_paging_enabled && ((cr0 & CR0_PG) == 0UL)) {
		if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
			/* Disable long mode */
			pr_dbg("VMM: Disable long mode");
			entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
			entry_ctrls &= ~VMX_ENTRY_CTLS_IA32E_MODE;
			exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

			vcpu_set_efer(vcpu,
				vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_LMA_BIT);
		} else {
		}
	}

	/* If CR0.CD or CR0.NW get cr0_changed_bits */
	if ((cr0_changed_bits & (CR0_CD | CR0_NW)) != 0UL) {
		/* No action if only CR0.NW is cr0_changed_bits */
		if ((cr0_changed_bits & CR0_CD) != 0UL) {
			if ((cr0 & CR0_CD) != 0UL) {
				/*
				 * When the guest requests to set CR0.CD, we don't allow
				 * guest's CR0.CD to be actually set, instead, we write guest
				 * IA32_PAT with all-UC entries to emulate the cache
				 * disabled behavior
				 */
				exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, PAT_ALL_UC_VALUE);
				if(!iommu_snoop_supported(vcpu->vm)) {
					cache_flush_invalidate_all();
				}
			} else {
				/* Restore IA32_PAT to enable cache again */
				exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL,
					vcpu_get_pat_ext(vcpu));
			}
			vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
		}
	}

	if ((cr0_changed_bits & (CR0_PG | CR0_WP)) != 0UL) {
		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	/* CR0 has no always off bits, except the always on bits, and reserved
	 * bits, allow to set according to guest.
	 */
	cr0_vmx = cr0_always_on_mask | cr0;

	/* Don't set CD or NW bit to guest */
	cr0_vmx &= ~(CR0_CD | CR0_NW);
	exec_vmwrite(VMX_GUEST_CR0, cr0_vmx & 0xFFFFFFFFUL);
	exec_vmwrite(VMX_CR0_READ_SHADOW, cr0 & 0xFFFFFFFFUL);

	/* clear read cache, next time read should from VMCS */
	bitmap_clear_lock(CPU_REG_CR0, &vcpu->reg_cached);

	pr_dbg("VMM: Try to write %016llx, allow to write 0x%016llx to CR0",
		cr0, cr0_vmx);
}

static bool is_cr4_write_valid(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	/* Check if guest try to set fixed to 0 bits or reserved bits */
	if ((cr4 & cr4_always_off_mask) != 0U) {
		return false;
	}

	/* Do NOT support nested guest */
	if ((cr4 & CR4_VMXE) != 0UL) {
		return false;
	}

	/* Do NOT support PCID in guest */
	if ((cr4 & CR4_PCIDE) != 0UL) {
		return false;
	}

	if (is_long_mode(vcpu)) {
		if ((cr4 & CR4_PAE) == 0UL) {
			return false;
		}
	}

	return true;
}

/*
 * Handling of CR4:
 * Assume "unrestricted guest" feature is supported by vmx.
 *
 * For CR4, if some feature is not supported by hardware, the corresponding bit
 * will be set in cr4_always_off_mask. If guest try to set these bits after
 * vmexit, will inject a #GP.
 * If a bit for a feature not supported by hardware, which is flexible to guest,
 * and write to it do not lead to a VM exit, a #GP should be generated inside
 * guest.
 *
 *   - VME (0) Flexible to guest
 *   - PVI (1) Flexible to guest
 *   - TSD (2) Flexible to guest
 *   - DE  (3) Flexible to guest
 *   - PSE (4) Trapped to track paging mode.
 *             Set the value according to the value from guest.
 *   - PAE (5) Trapped to track paging mode.
 *             Set the value according to the value from guest.
 *   - MCE (6) Flexible to guest
 *   - PGE (7) Flexible to guest
 *   - PCE (8) Flexible to guest
 *   - OSFXSR (9) Flexible to guest
 *   - OSXMMEXCPT (10) Flexible to guest
 *   - VMXE (13) Trapped to hide from guest
 *   - SMXE (14) must always be 0 => must lead to a VM exit
 *   - PCIDE (17) Trapped to hide from guest
 *   - OSXSAVE (18) Flexible to guest
 *   - XSAVE (19) Flexible to guest
 *   		We always keep align with physical cpu. So it's flexible to
 *   		guest
 *   - SMEP (20) Flexible to guest
 *   - SMAP (21) Flexible to guest
 *   - PKE (22) Flexible to guest
 */
void vmx_write_cr4(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	uint64_t cr4_vmx;
	uint64_t old_cr4 = vcpu_get_cr4(vcpu);

	if (!is_cr4_write_valid(vcpu, cr4)) {
		pr_dbg("Invalid cr4 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
		return;
	}

	if (((cr4 ^ old_cr4) & (CR4_PGE | CR4_PSE | CR4_PAE |
			CR4_SMEP | CR4_SMAP | CR4_PKE)) != 0UL) {
		if (((cr4 & CR4_PAE) != 0UL) && is_paging_enabled(vcpu) &&
				(is_long_mode(vcpu))) {
			load_pdptrs(vcpu);
		}

		vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
	}

	/* Aways off bits and reserved bits has been filtered above */
	cr4_vmx = cr4_always_on_mask | cr4;
	exec_vmwrite(VMX_GUEST_CR4, cr4_vmx & 0xFFFFFFFFUL);
	exec_vmwrite(VMX_CR4_READ_SHADOW, cr4 & 0xFFFFFFFFUL);

	/* clear read cache, next time read should from VMCS */
	bitmap_clear_lock(CPU_REG_CR4, &vcpu->reg_cached);

	pr_dbg("VMM: Try to write %016llx, allow to write 0x%016llx to CR4",
		cr4, cr4_vmx);
}

/* rip, rsp, ia32_efer and rflags are written to VMCS in start_vcpu */
static void init_guest_vmx(struct acrn_vcpu *vcpu, uint64_t cr0, uint64_t cr3,
	uint64_t cr4)
{
	struct cpu_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context];
	struct ext_context *ectx = &ctx->ext_ctx;

	vcpu_set_cr4(vcpu, cr4);
	vcpu_set_cr0(vcpu, cr0);
	exec_vmwrite(VMX_GUEST_CR3, cr3);

	exec_vmwrite(VMX_GUEST_GDTR_BASE, ectx->gdtr.base);
	pr_dbg("VMX_GUEST_GDTR_BASE: 0x%016llx", ectx->gdtr.base);
	exec_vmwrite32(VMX_GUEST_GDTR_LIMIT, ectx->gdtr.limit);
	pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%016llx", ectx->gdtr.limit);

	exec_vmwrite(VMX_GUEST_IDTR_BASE, ectx->idtr.base);
	pr_dbg("VMX_GUEST_IDTR_BASE: 0x%016llx", ectx->idtr.base);
	exec_vmwrite32(VMX_GUEST_IDTR_LIMIT, ectx->idtr.limit);
	pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%016llx", ectx->idtr.limit);

	/* init segment selectors: es, cs, ss, ds, fs, gs, ldtr, tr */
	load_segment(ectx->cs, VMX_GUEST_CS);
	load_segment(ectx->ss, VMX_GUEST_SS);
	load_segment(ectx->ds, VMX_GUEST_DS);
	load_segment(ectx->es, VMX_GUEST_ES);
	load_segment(ectx->fs, VMX_GUEST_FS);
	load_segment(ectx->gs, VMX_GUEST_GS);
	load_segment(ectx->tr, VMX_GUEST_TR);
	load_segment(ectx->ldtr, VMX_GUEST_LDTR);

	/* fixed values */
	exec_vmwrite32(VMX_GUEST_IA32_SYSENTER_CS, 0U);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_ESP, 0UL);
	exec_vmwrite(VMX_GUEST_IA32_SYSENTER_EIP, 0UL);
	exec_vmwrite(VMX_GUEST_PENDING_DEBUG_EXCEPT, 0UL);
	exec_vmwrite(VMX_GUEST_IA32_DEBUGCTL_FULL, 0UL);
	exec_vmwrite32(VMX_GUEST_INTERRUPTIBILITY_INFO, 0U);
	exec_vmwrite32(VMX_GUEST_ACTIVITY_STATE, 0U);
	exec_vmwrite32(VMX_GUEST_SMBASE, 0U);
	vcpu_set_pat_ext(vcpu, PAT_POWER_ON_VALUE);
	exec_vmwrite(VMX_GUEST_IA32_PAT_FULL, PAT_POWER_ON_VALUE);
	exec_vmwrite(VMX_GUEST_DR7, DR7_INIT_VALUE);
}

static void init_guest_state(struct acrn_vcpu *vcpu)
{
	struct cpu_context *ctx =
		&vcpu->arch.contexts[vcpu->arch.cur_context];

	init_guest_vmx(vcpu, ctx->run_ctx.cr0, ctx->ext_ctx.cr3,
			ctx->run_ctx.cr4 & ~CR4_VMXE);
}

static void init_host_state(void)
{
	uint16_t value16;
	uint64_t value64;
	uint64_t value;
	uint64_t tss_addr;
	uint64_t gdt_base;
	uint64_t idt_base;

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
	CPU_SEG_READ(es, &value16);
	exec_vmwrite16(VMX_HOST_ES_SEL, value16);
	pr_dbg("VMX_HOST_ES_SEL: 0x%hx ", value16);

	CPU_SEG_READ(cs, &value16);
	exec_vmwrite16(VMX_HOST_CS_SEL, value16);
	pr_dbg("VMX_HOST_CS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(ss, &value16);
	exec_vmwrite16(VMX_HOST_SS_SEL, value16);
	pr_dbg("VMX_HOST_SS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(ds, &value16);
	exec_vmwrite16(VMX_HOST_DS_SEL, value16);
	pr_dbg("VMX_HOST_DS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(fs, &value16);
	exec_vmwrite16(VMX_HOST_FS_SEL, value16);
	pr_dbg("VMX_HOST_FS_SEL: 0x%hx ", value16);

	CPU_SEG_READ(gs, &value16);
	exec_vmwrite16(VMX_HOST_GS_SEL, value16);
	pr_dbg("VMX_HOST_GS_SEL: 0x%hx ", value16);

	exec_vmwrite16(VMX_HOST_TR_SEL, HOST_GDT_RING0_CPU_TSS_SEL);
	pr_dbg("VMX_HOST_TR_SEL: 0x%hx ", HOST_GDT_RING0_CPU_TSS_SEL);

	/******************************************************
	 * 32-bit fields
	 * Set up the 32 bit host state fields - pg 3418 B.3.3 * Set limit for
	 * ES, CS, DD, DS, FS, GS, LDTR, Guest TR, * GDTR, and IDTR
	 ******************************************************/

	/* TODO: Should guest GDTB point to host GDTB ? */
	/* Obtain the current global descriptor table base */
	gdt_base = sgdt();

	if (((gdt_base >> 47U) & 0x1UL) != 0UL) {
	        gdt_base |= 0xffff000000000000UL;
	}

	/* Set up the guest and host GDTB base fields with current GDTB base */
	exec_vmwrite(VMX_HOST_GDTR_BASE, gdt_base);
	pr_dbg("VMX_HOST_GDTR_BASE: 0x%x ", gdt_base);

	tss_addr = hva2hpa((void *)&get_cpu_var(tss));
	/* Set up host TR base fields */
	exec_vmwrite(VMX_HOST_TR_BASE, tss_addr);
	pr_dbg("VMX_HOST_TR_BASE: 0x%016llx ", tss_addr);

	/* Obtain the current interrupt descriptor table base */
	idt_base = sidt();
	/* base */
	if (((idt_base >> 47U) & 0x1UL) != 0UL) {
		idt_base |= 0xffff000000000000UL;
	}

	exec_vmwrite(VMX_HOST_IDTR_BASE, idt_base);
	pr_dbg("VMX_HOST_IDTR_BASE: 0x%x ", idt_base);

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
	exec_vmwrite(VMX_HOST_CR0, value);
	pr_dbg("VMX_HOST_CR0: 0x%016llx ", value);

	/* Set up host CR3 field */
	CPU_CR_READ(cr3, &value);
	exec_vmwrite(VMX_HOST_CR3, value);
	pr_dbg("VMX_HOST_CR3: 0x%016llx ", value);

	/* Set up host CR4 field */
	CPU_CR_READ(cr4, &value);
	exec_vmwrite(VMX_HOST_CR4, value);
	pr_dbg("VMX_HOST_CR4: 0x%016llx ", value);

	/* Set up host and guest FS base address */
	value = msr_read(MSR_IA32_FS_BASE);
	exec_vmwrite(VMX_HOST_FS_BASE, value);
	pr_dbg("VMX_HOST_FS_BASE: 0x%016llx ", value);
	value = msr_read(MSR_IA32_GS_BASE);
	exec_vmwrite(VMX_HOST_GS_BASE, value);
	pr_dbg("VMX_HOST_GS_BASE: 0x%016llx ", value);

	/* Set up host instruction pointer on VM Exit */
	value64 = (uint64_t)&vm_exit;
	pr_dbg("HOST RIP on VMExit %016llx ", value64);
	exec_vmwrite(VMX_HOST_RIP, value64);
	pr_dbg("vm exit return address = %016llx ", value64);

	/* As a type I hypervisor, just init sysenter fields to 0 */
	exec_vmwrite32(VMX_HOST_IA32_SYSENTER_CS, 0U);
	exec_vmwrite(VMX_HOST_IA32_SYSENTER_ESP, 0UL);
	exec_vmwrite(VMX_HOST_IA32_SYSENTER_EIP, 0UL);
}

static uint32_t check_vmx_ctrl(uint32_t msr, uint32_t ctrl_req)
{
	uint64_t vmx_msr;
	uint32_t vmx_msr_low, vmx_msr_high;
	uint32_t ctrl = ctrl_req;

	vmx_msr = msr_read(msr);
	vmx_msr_low  = (uint32_t)vmx_msr;
	vmx_msr_high = (uint32_t)(vmx_msr >> 32U);
	pr_dbg("VMX_PIN_VM_EXEC_CONTROLS:low=0x%x, high=0x%x\n",
			vmx_msr_low, vmx_msr_high);

	/* high 32b: must 0 setting
	 * low 32b:  must 1 setting
	 */
	ctrl &= vmx_msr_high;
	ctrl |= vmx_msr_low;

	if ((ctrl_req & ~ctrl) != 0U) {
		pr_err("VMX ctrl 0x%x not fully enabled: "
			"request 0x%x but get 0x%x\n",
			msr, ctrl_req, ctrl);
	}

	return ctrl;

}

static void init_exec_ctrl(struct acrn_vcpu *vcpu)
{
	uint32_t value32;
	uint64_t value64;
	struct acrn_vm *vm = vcpu->vm;

	/* Log messages to show initializing VMX execution controls */
	pr_dbg("*****************************");
	pr_dbg("Initialize execution control ");
	pr_dbg("*****************************");

	/* Set up VM Execution control to enable Set VM-exits on external
	 * interrupts preemption timer - pg 2899 24.6.1
	 */
	/* enable external interrupt VM Exit */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PINBASED_CTLS,
			VMX_PINBASED_CTLS_IRQ_EXIT);

	if (is_apicv_posted_intr_supported()) {
		value32 |= VMX_PINBASED_CTLS_POST_IRQ;
	}

	exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, value32);
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
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS,
			VMX_PROCBASED_CTLS_TSC_OFF |
			/* VMX_PROCBASED_CTLS_RDTSC | */
			VMX_PROCBASED_CTLS_TPR_SHADOW|
			VMX_PROCBASED_CTLS_IO_BITMAP |
			VMX_PROCBASED_CTLS_MSR_BITMAP |
			VMX_PROCBASED_CTLS_SECONDARY);

	/*Disable VM_EXIT for CR3 access*/
	value32 &= ~(VMX_PROCBASED_CTLS_CR3_LOAD |
			VMX_PROCBASED_CTLS_CR3_STORE);

	/*
	 * Disable VM_EXIT for invlpg execution.
	 */
	value32 &= ~VMX_PROCBASED_CTLS_INVLPG;

	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS: 0x%x ", value32);

	/* Set up secondary processor based VM execution controls - pg 2901
	 * 24.6.2. Set up for: * Enable EPT * Enable RDTSCP * Unrestricted
	 * guest (optional)
	 */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS2,
			VMX_PROCBASED_CTLS2_VAPIC |
			VMX_PROCBASED_CTLS2_EPT |
			VMX_PROCBASED_CTLS2_RDTSCP |
			VMX_PROCBASED_CTLS2_UNRESTRICT|
			VMX_PROCBASED_CTLS2_VAPIC_REGS);

	if (vcpu->arch.vpid != 0U) {
		value32 |= VMX_PROCBASED_CTLS2_VPID;
	} else {
		value32 &= ~VMX_PROCBASED_CTLS2_VPID;
	}

	if (is_apicv_intr_delivery_supported()) {
		value32 |= VMX_PROCBASED_CTLS2_VIRQ;
	} else {
		/*
		 * This field exists only on processors that support
		 * the 1-setting  of the "use TPR shadow"
		 * VM-execution control.
		 *
		 * Set up TPR threshold for virtual interrupt delivery
		 * - pg 2904 24.6.8
		 */
		exec_vmwrite32(VMX_TPR_THRESHOLD, 0U);
	}

	if (cpu_has_cap(X86_FEATURE_OSXSAVE)) {
		exec_vmwrite64(VMX_XSS_EXITING_BITMAP_FULL, 0UL);
		value32 |= VMX_PROCBASED_CTLS2_XSVE_XRSTR;
	}

	value32 |= VMX_PROCBASED_CTLS2_WBINVD;

	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS2: 0x%x ", value32);

	/*APIC-v, config APIC-access address*/
	value64 = vlapic_apicv_get_apic_access_addr();
	exec_vmwrite64(VMX_APIC_ACCESS_ADDR_FULL, value64);

	/*APIC-v, config APIC virtualized page address*/
	value64 = vlapic_apicv_get_apic_page_addr(vcpu_vlapic(vcpu));
	exec_vmwrite64(VMX_VIRTUAL_APIC_PAGE_ADDR_FULL, value64);

	if (is_apicv_intr_delivery_supported()) {
		/* Disable all EOI VMEXIT by default and
		 * clear RVI and SVI.
		 */
		exec_vmwrite64(VMX_EOI_EXIT0_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT1_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT2_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT3_FULL, 0UL);

		exec_vmwrite16(VMX_GUEST_INTR_STATUS, 0U);
		if (is_apicv_posted_intr_supported()) {
			exec_vmwrite16(VMX_POSTED_INTR_VECTOR,
					VECTOR_POSTED_INTR);
			exec_vmwrite64(VMX_PIR_DESC_ADDR_FULL,
					apicv_get_pir_desc_paddr(vcpu));
		}
	}

	/* Load EPTP execution control
	 * TODO: introduce API to make this data driven based
	 * on VMX_EPT_VPID_CAP
	 */
	value64 = hva2hpa(vm->arch_vm.nworld_eptp) | (3UL << 3U) | 6UL;
	exec_vmwrite64(VMX_EPT_POINTER_FULL, value64);
	pr_dbg("VMX_EPT_POINTER: 0x%016llx ", value64);

	/* Set up guest exception mask bitmap setting a bit * causes a VM exit
	 * on corresponding guest * exception - pg 2902 24.6.3
	 * enable VM exit on MC only
	 */
	value32 = (1U << IDT_MC);
	exec_vmwrite32(VMX_EXCEPTION_BITMAP, value32);

	/* Set up page fault error code mask - second paragraph * pg 2902
	 * 24.6.3 - guest page fault exception causing * vmexit is governed by
	 * both VMX_EXCEPTION_BITMAP and * VMX_PF_ERROR_CODE_MASK
	 */
	exec_vmwrite32(VMX_PF_ERROR_CODE_MASK, 0U);

	/* Set up page fault error code match - second paragraph * pg 2902
	 * 24.6.3 - guest page fault exception causing * vmexit is governed by
	 * both VMX_EXCEPTION_BITMAP and * VMX_PF_ERROR_CODE_MATCH
	 */
	exec_vmwrite32(VMX_PF_ERROR_CODE_MATCH, 0U);

	/* Set up CR3 target count - An execution of mov to CR3 * by guest
	 * causes HW to evaluate operand match with * one of N CR3-Target Value
	 * registers. The CR3 target * count values tells the number of
	 * target-value regs to evaluate
	 */
	exec_vmwrite32(VMX_CR3_TARGET_COUNT, 0U);

	/* Set up IO bitmap register A and B - pg 2902 24.6.4 */
	value64 = hva2hpa(vm->arch_vm.io_bitmap);
	exec_vmwrite64(VMX_IO_BITMAP_A_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_A: 0x%016llx ", value64);
	value64 = hva2hpa(&(vm->arch_vm.io_bitmap[CPU_PAGE_SIZE]));
	exec_vmwrite64(VMX_IO_BITMAP_B_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_B: 0x%016llx ", value64);

	init_msr_emulation(vcpu);

	/* Set up executive VMCS pointer - pg 2905 24.6.10 */
	exec_vmwrite64(VMX_EXECUTIVE_VMCS_PTR_FULL, 0UL);

	/* Setup Time stamp counter offset - pg 2902 24.6.5 */
	exec_vmwrite64(VMX_TSC_OFFSET_FULL, 0UL);

	/* Set up the link pointer */
	exec_vmwrite64(VMX_VMS_LINK_PTR_FULL, 0xFFFFFFFFFFFFFFFFUL);

	/* Natural-width */
	pr_dbg("Natural-width*********");

	init_cr0_cr4_host_mask();

	/* The CR3 target registers work in concert with VMX_CR3_TARGET_COUNT
	 * field. Using these registers guest CR3 access can be managed. i.e.,
	 * if operand does not match one of these register values a VM exit
	 * would occur
	 */
	exec_vmwrite(VMX_CR3_TARGET_0, 0UL);
	exec_vmwrite(VMX_CR3_TARGET_1, 0UL);
	exec_vmwrite(VMX_CR3_TARGET_2, 0UL);
	exec_vmwrite(VMX_CR3_TARGET_3, 0UL);
}

static void init_entry_ctrl(const struct acrn_vcpu *vcpu)
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
	value32 = (VMX_ENTRY_CTLS_LOAD_EFER |
		   VMX_ENTRY_CTLS_LOAD_PAT);

	if (get_vcpu_mode(vcpu) == CPU_MODE_64BIT) {
		value32 |= (VMX_ENTRY_CTLS_IA32E_MODE);
	}

	value32 = check_vmx_ctrl(MSR_IA32_VMX_ENTRY_CTLS, value32);

	exec_vmwrite32(VMX_ENTRY_CONTROLS, value32);
	pr_dbg("VMX_ENTRY_CONTROLS: 0x%x ", value32);

	/* Set up VMX entry MSR load count - pg 2908 24.8.2 Tells the number of
	 * MSRs on load from memory on VM entry from mem address provided by
	 * VM-entry MSR load address field
	 */
	exec_vmwrite32(VMX_ENTRY_MSR_LOAD_COUNT, MSR_AREA_COUNT);
	exec_vmwrite64(VMX_ENTRY_MSR_LOAD_ADDR_FULL, (uint64_t)vcpu->arch.msr_area.guest);

	/* Set up VM entry interrupt information field pg 2909 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, 0U);

	/* Set up VM entry exception error code - pg 2910 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE, 0U);

	/* Set up VM entry instruction length - pg 2910 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_INSTR_LENGTH, 0U);
}

static void init_exit_ctrl(struct acrn_vcpu *vcpu)
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
	value32 = check_vmx_ctrl(MSR_IA32_VMX_EXIT_CTLS,
			VMX_EXIT_CTLS_ACK_IRQ |
			VMX_EXIT_CTLS_SAVE_PAT |
			VMX_EXIT_CTLS_LOAD_PAT |
			VMX_EXIT_CTLS_LOAD_EFER |
			VMX_EXIT_CTLS_SAVE_EFER |
			VMX_EXIT_CTLS_HOST_ADDR64);

	exec_vmwrite32(VMX_EXIT_CONTROLS, value32);
	pr_dbg("VMX_EXIT_CONTROL: 0x%x ", value32);

	/* Set up VM exit MSR store and load counts pg 2908 24.7.2 - tells the
	 * HW number of MSRs to stored to mem and loaded from mem on VM exit.
	 * The 64 bit VM-exit MSR store and load address fields provide the
	 * corresponding addresses
	 */
	exec_vmwrite32(VMX_EXIT_MSR_STORE_COUNT, MSR_AREA_COUNT);
	exec_vmwrite32(VMX_EXIT_MSR_LOAD_COUNT, MSR_AREA_COUNT);
	exec_vmwrite64(VMX_EXIT_MSR_STORE_ADDR_FULL, (uint64_t)vcpu->arch.msr_area.guest);
	exec_vmwrite64(VMX_EXIT_MSR_LOAD_ADDR_FULL, (uint64_t)vcpu->arch.msr_area.host);
}

/**
 * @pre vcpu != NULL
 */
void init_vmcs(struct acrn_vcpu *vcpu)
{
	uint64_t vmx_rev_id;
	uint64_t vmcs_pa;

	/* Log message */
	pr_dbg("Initializing VMCS");

	/* Obtain the VM Rev ID from HW and populate VMCS page with it */
	vmx_rev_id = msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s(vcpu->arch.vmcs, 4U, (void *)&vmx_rev_id, 4U);

	/* Execute VMCLEAR on current VMCS */
	vmcs_pa = hva2hpa(vcpu->arch.vmcs);
	exec_vmclear((void *)&vmcs_pa);

	/* Load VMCS pointer */
	exec_vmptrld((void *)&vmcs_pa);

	/* Initialize the Virtual Machine Control Structure (VMCS) */
	init_host_state();
	/* init exec_ctrl needs to run before init_guest_state */
	init_exec_ctrl(vcpu);
	init_guest_state(vcpu);
	init_entry_ctrl(vcpu);
	init_exit_ctrl(vcpu);
}

#ifndef CONFIG_PARTITION_MODE
void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu)
{
	uint32_t value32;
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2);
	value32 &= ~VMX_PROCBASED_CTLS2_VAPIC;
	value32 |= VMX_PROCBASED_CTLS2_VX2APIC;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
	update_msr_bitmap_x2apic_apicv(vcpu);
}
#else
void switch_apicv_mode_x2apic(struct acrn_vcpu *vcpu)
{
	uint32_t value32;
	if(vcpu->vm->vm_desc->lapic_pt) {
		/*
		 * Disable external interrupt exiting and irq ack
		 * Disable posted interrupt processing
		 * update x2apic msr bitmap for pass-thru
		 * enable inteception only for ICR
		 * disable pre-emption for TSC DEADLINE MSR
		 * Disable Register Virtualization and virtual interrupt delivery
		 * Disable "use TPR shadow"
		 */

		value32 = exec_vmread32(VMX_PIN_VM_EXEC_CONTROLS);
		value32 &= ~VMX_PINBASED_CTLS_IRQ_EXIT;
		if (is_apicv_posted_intr_supported()) {
			value32 &= ~VMX_PINBASED_CTLS_POST_IRQ;
		}
		exec_vmwrite32(VMX_PIN_VM_EXEC_CONTROLS, value32);

		value32 = exec_vmread32(VMX_EXIT_CONTROLS);
		value32 &= ~VMX_EXIT_CTLS_ACK_IRQ;
		exec_vmwrite32(VMX_EXIT_CONTROLS, value32);

		value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
		value32 &= ~VMX_PROCBASED_CTLS_TPR_SHADOW;
		exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);

		exec_vmwrite32(VMX_TPR_THRESHOLD, 0U);

		value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2);
		value32 &= ~VMX_PROCBASED_CTLS2_VAPIC_REGS;
		if (is_apicv_intr_delivery_supported()) {
			value32 &= ~VMX_PROCBASED_CTLS2_VIRQ;
		}
		exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);

		update_msr_bitmap_x2apic_passthru(vcpu);
	} else {
		value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS2);
		value32 &= ~VMX_PROCBASED_CTLS2_VAPIC;
		value32 |= VMX_PROCBASED_CTLS2_VX2APIC;
		exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
		update_msr_bitmap_x2apic_apicv(vcpu);
	}
}
#endif
