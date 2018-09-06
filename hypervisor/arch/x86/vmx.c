/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <vm0_boot.h>
#ifdef CONFIG_EFI_STUB
extern struct boot_ctx* efi_ctx;
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
	uint64_t tmp64;

	/* Read Feature ControL MSR */
	tmp64 = msr_read(MSR_IA32_FEATURE_CONTROL);

	/* Check if feature control is locked */
	if ((tmp64 & MSR_IA32_FEATURE_CONTROL_LOCK) == 0U) {
		/* Lock and enable VMX support */
		tmp64 |= (MSR_IA32_FEATURE_CONTROL_LOCK |
			  MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX);
		msr_write(MSR_IA32_FEATURE_CONTROL, tmp64);
	}

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
	struct vcpu *vcpu = get_ever_run_vcpu(pcpu_id);

	/* Initialize vmxon page with revision id from IA32 VMX BASIC MSR */
	tmp32 = (uint32_t)msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s((uint32_t *) vmxon_region_va, 4U, (void *)&tmp32, 4U);

	/* Turn on CR0.NE and CR4.VMXE */
	CPU_CR_READ(cr0, &tmp64);
	CPU_CR_WRITE(cr0, tmp64 | CR0_NE);
	CPU_CR_READ(cr4, &tmp64);
	CPU_CR_WRITE(cr4, tmp64 | CR4_VMXE);

	/* Turn ON VMX */
	vmxon_region_pa = hva2hpa(vmxon_region_va);
	exec_vmxon(&vmxon_region_pa);

	if (vcpu != NULL) {
		vmcs_pa = hva2hpa(vcpu->arch_vcpu.vmcs);
		exec_vmptrld(&vmcs_pa);
	}
}

void vmx_off(uint16_t pcpu_id)
{

	struct vcpu *vcpu = get_ever_run_vcpu(pcpu_id);
	uint64_t vmcs_pa;

	if (vcpu != NULL) {
		vmcs_pa = hva2hpa(vcpu->arch_vcpu.vmcs);
		exec_vmclear((void *)&vmcs_pa);
	}

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

static void init_cr0_cr4_host_mask(__unused struct vcpu *vcpu)
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

uint64_t vmx_rdmsr_pat(struct vcpu *vcpu)
{
	/*
	 * note: if context->cr0.CD is set, the actual value in guest's
	 * IA32_PAT MSR is PAT_ALL_UC_VALUE, which may be different from
	 * the saved value saved_context->ia32_pat
	 */
	return vcpu_get_pat_ext(vcpu);
}

int vmx_wrmsr_pat(struct vcpu *vcpu, uint64_t value)
{
	uint32_t i;
	uint64_t field;

	for (i = 0U; i < 8U; i++) {
		field = (value >> (i * 8U)) & 0xffUL;
		if (PAT_MEM_TYPE_INVALID(field) ||
				((PAT_FIELD_RSV_BITS & field) != 0UL)) {
			pr_err("invalid guest IA32_PAT: 0x%016llx", value);
			vcpu_inject_gp(vcpu, 0U);
			return 0;
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

static bool is_cr0_write_valid(struct vcpu *vcpu, uint64_t cr0)
{
	/* Shouldn't set always off bit */
	if ((cr0 & cr0_always_off_mask) != 0UL)
		return false;

	/* SDM 25.3 "Changes to instruction behavior in VMX non-root"
	 *
	 * We always require "unrestricted guest" control enabled. So
	 *
	 * CR0.PG = 1, CR4.PAE = 0 and IA32_EFER.LME = 1 is invalid.
	 * CR0.PE = 0 and CR0.PG = 1 is invalid.
	 */
	if (((cr0 & CR0_PG) != 0UL) && ((vcpu_get_cr4(vcpu) & CR4_PAE) == 0UL)
		&& ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL))
		return false;

	if (((cr0 & CR0_PE) == 0UL) && ((cr0 & CR0_PG) != 0UL))
		return false;

	/* SDM 6.15 "Exception and Interrupt Refrerence" GP Exception
	 *
	 * Loading CR0 regsiter with a set NW flag and a clear CD flag
	 * is invalid
	 */
	if (((cr0 & CR0_CD) == 0UL) && ((cr0 & CR0_NW) != 0UL))
		return false;

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
void vmx_write_cr0(struct vcpu *vcpu, uint64_t cr0)
{
	uint64_t cr0_vmx;
	uint32_t entry_ctrls;
	bool paging_enabled = !!(vcpu_get_cr0(vcpu) & CR0_PG);

	if (!is_cr0_write_valid(vcpu, cr0)) {
		pr_dbg("Invalid cr0 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
		return;
	}

	/* SDM 2.5
	 * When loading a control register, reserved bit should always set
	 * to the value previously read.
	 */
	cr0 = (cr0 & ~CR0_RESERVED_MASK) |
		(vcpu_get_cr0(vcpu) & CR0_RESERVED_MASK);

	if (((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) &&
	    !paging_enabled && ((cr0 & CR0_PG) != 0UL)) {
		/* Enable long mode */
		pr_dbg("VMM: Enable long mode");
		entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
		entry_ctrls |= VMX_ENTRY_CTLS_IA32E_MODE;
		exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

		vcpu_set_efer(vcpu,
			vcpu_get_efer(vcpu) | MSR_IA32_EFER_LMA_BIT);
	} else if (((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) &&
		   paging_enabled && ((cr0 & CR0_PG) == 0UL)){
		/* Disable long mode */
		pr_dbg("VMM: Disable long mode");
		entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
		entry_ctrls &= ~VMX_ENTRY_CTLS_IA32E_MODE;
		exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

		vcpu_set_efer(vcpu,
			vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_LMA_BIT);
	} else {
		/* CR0.PG unchanged. */
	}

	/* If CR0.CD or CR0.NW get changed */
	if (((vcpu_get_cr0(vcpu) ^ cr0) & (CR0_CD | CR0_NW)) != 0UL) {
		/* No action if only CR0.NW is changed */
		if (((vcpu_get_cr0(vcpu) ^ cr0) & CR0_CD) != 0UL) {
			if ((cr0 & CR0_CD) != 0UL) {
				/*
				 * When the guest requests to set CR0.CD, we don't allow
				 * guest's CR0.CD to be actually set, instead, we write guest
				 * IA32_PAT with all-UC entries to emulate the cache
				 * disabled behavior
				 */
				exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, PAT_ALL_UC_VALUE);
				CACHE_FLUSH_INVALIDATE_ALL();
			} else {
				/* Restore IA32_PAT to enable cache again */
				exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL,
					vcpu_get_pat_ext(vcpu));
			}
			vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
		}
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

static bool is_cr4_write_valid(uint64_t cr4)
{
	/* Check if guest try to set fixed to 0 bits or reserved bits */
	if ((cr4 & cr4_always_off_mask) != 0U)
		return false;

	/* Do NOT support nested guest */
	if ((cr4 & CR4_VMXE) != 0UL)
		return false;

	/* Do NOT support PCID in guest */
	if ((cr4 & CR4_PCIDE) != 0UL)
		return false;

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
void vmx_write_cr4(struct vcpu *vcpu, uint64_t cr4)
{
	uint64_t cr4_vmx;

	if (!is_cr4_write_valid(cr4)) {
		pr_dbg("Invalid cr4 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
		return;
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

static void init_guest_context_real(struct vcpu *vcpu)
{
	struct ext_context *ectx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].ext_ctx;
	struct segment_sel *seg;

	/* cs, ss, ds, es, fs, gs; cs will be override later. */
	for (seg = &(ectx->cs); seg <= &(ectx->gs); seg++) {
		seg->selector = 0U;
		seg->base = 0UL;
		seg->limit = 0xFFFFU;
		seg->attr = REAL_MODE_DATA_SEG_AR;
	}

	if (is_vcpu_bsp(vcpu)) {
		/* There are two cases that we will start bsp in real
		 * mode:
		 * 1. UOS start
		 * 2. SOS resume from S3
		 *
		 * For 1, DM will set correct entry_addr.
		 * For 2, SOS resume caller will set entry_addr to
		 *        SOS wakeup vec. According to ACPI FACS spec,
		 *        wakeup vec should be < 1MB. So we use < 1MB
		 *        to detect whether it's resume from S3 and we
		 *        setup CS:IP to
		 *        (wakeup_vec >> 4):(wakeup_vec & 0x000F)
		 *        if it's resume from S3.
		 *
		 */
		if ((uint64_t)vcpu->entry_addr < 0x100000UL) {
			ectx->cs.selector = (uint16_t)
				(((uint64_t)vcpu->entry_addr & 0xFFFF0UL) >> 4U);
			ectx->cs.base = (uint64_t)ectx->cs.selector << 4U;
			vcpu_set_rip(vcpu, (uint64_t)vcpu->entry_addr & 0x0FUL);
		} else {
			/* BSP is initialized with real mode */
			ectx->cs.selector = REAL_MODE_BSP_INIT_CODE_SEL;
			/* For unrestricted guest, it is able
			 * to set a high base address
			 */
			ectx->cs.base = (uint64_t)vcpu->entry_addr & 0xFFFF0000UL;
			vcpu_set_rip(vcpu, 0x0000FFF0UL);
		}
	} else {
		/* AP is initialized with real mode
		 * and CS value is left shift 8 bits from sipi vector.
		 */
		ectx->cs.selector = (uint16_t)(vcpu->arch_vcpu.sipi_vector << 8U);
		ectx->cs.base = (uint64_t)ectx->cs.selector << 4U;
	}
	ectx->cs.attr = REAL_MODE_CODE_SEG_AR;

	ectx->gdtr.base = 0UL;
	ectx->gdtr.limit = 0xFFFFU;
	ectx->idtr.base = 0UL;
	ectx->idtr.limit = 0xFFFFU;
}

static void init_guest_context_vm0_bsp(struct vcpu *vcpu)
{
	struct ext_context *ectx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].ext_ctx;
	struct boot_ctx * init_ctx = (struct boot_ctx *)(&vm0_boot_context);
	uint16_t *sel = &(init_ctx->cs_sel);
	struct segment_sel *seg;

	for (seg = &(ectx->cs); seg <= &(ectx->gs); seg++) {
		seg->base     = 0UL;
		seg->limit    = 0xFFFFFFFFU;
		seg->attr     = PROTECTED_MODE_DATA_SEG_AR;
		seg->selector = *sel;
		sel++;
	}
	ectx->cs.attr         = init_ctx->cs_ar;	/* override cs attr */

	vcpu_set_rip(vcpu, (uint64_t)vcpu->entry_addr);
	vcpu_set_efer(vcpu, init_ctx->ia32_efer);

	ectx->gdtr.base      = init_ctx->gdt.base;
	ectx->gdtr.limit     = init_ctx->gdt.limit;

	ectx->idtr.base      = init_ctx->idt.base;
	ectx->idtr.limit     = init_ctx->idt.limit;

	ectx->ldtr.selector  = init_ctx->ldt_sel;
	ectx->tr.selector    = init_ctx->tr_sel;
#ifdef CONFIG_EFI_STUB
	vcpu_set_rsp(vcpu, efi_ctx->gprs.rsp);
	/* clear flags for CF/PF/AF/ZF/SF/OF */
	vcpu_set_rflags(vcpu, efi_ctx->rflags & ~(0x8d5UL));
#endif
}

/* only be called for UOS when bsp start from protected mode */
static void init_guest_context_protect(struct vcpu *vcpu)
{
	struct ext_context *ectx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].ext_ctx;
	struct segment_sel *seg;

	ectx->gdtr.base = create_guest_init_gdt(vcpu->vm, &ectx->gdtr.limit);
	for (seg = &(ectx->cs); seg <= &(ectx->gs); seg++) {
		seg->base = 0UL;
		seg->limit = 0xFFFFFFFFU;
		seg->attr = PROTECTED_MODE_DATA_SEG_AR;
		seg->selector = 0x18U;
	}
	ectx->cs.attr = PROTECTED_MODE_CODE_SEG_AR;
	ectx->cs.selector = 0x10U; /* Linear code segment */
	vcpu_set_rip(vcpu, (uint64_t)vcpu->entry_addr);
}

/* rip, rsp, ia32_efer and rflags are written to VMCS in start_vcpu */
static void init_guest_vmx(struct vcpu *vcpu, uint64_t cr0, uint64_t cr3,
	uint64_t cr4)
{
	struct cpu_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
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

static void init_guest_state(struct vcpu *vcpu)
{
	struct cpu_context *ctx =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	struct boot_ctx * init_ctx = (struct boot_ctx *)(&vm0_boot_context);
	enum vm_cpu_mode vcpu_mode = get_vcpu_mode(vcpu);

	vcpu_set_rflags(vcpu, 0x2UL); /* Bit 1 is a active high reserved bit */

	/* ldtr */
	ctx->ext_ctx.ldtr.selector = 0U;
	ctx->ext_ctx.ldtr.base = 0UL;
	ctx->ext_ctx.ldtr.limit = 0xFFFFU;
	ctx->ext_ctx.ldtr.attr = LDTR_AR;
	/* tr */
	ctx->ext_ctx.tr.selector = 0U;
	ctx->ext_ctx.tr.base = 0UL;
	ctx->ext_ctx.tr.limit = 0xFFFFU;
	ctx->ext_ctx.tr.attr = TR_AR;

	if (vcpu_mode == CPU_MODE_REAL) {
		init_guest_context_real(vcpu);
		init_guest_vmx(vcpu, CR0_ET | CR0_NE, 0, 0);
	} else if (is_vm0(vcpu->vm) && is_vcpu_bsp(vcpu)) {
		init_guest_context_vm0_bsp(vcpu);
		init_guest_vmx(vcpu, init_ctx->cr0, init_ctx->cr3,
			       init_ctx->cr4 & ~CR4_VMXE);
	} else {
		init_guest_context_protect(vcpu);
		init_guest_vmx(vcpu, CR0_ET | CR0_NE | CR0_PE, 0, 0);
	}
}

static void init_host_state(__unused struct vcpu *vcpu)
{
	uint16_t value16;
	uint64_t value64;
	uint64_t value;
	uint64_t trbase;
	uint64_t trbase_lo;
	uint64_t trbase_hi;
	uint64_t realtrbase;
	descriptor_table gdtb = {0U, 0UL};
	descriptor_table idtb = {0U, 0UL};
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
	asm volatile ("movw %%es, %%ax":"=a" (value16));
	exec_vmwrite16(VMX_HOST_ES_SEL, value16);
	pr_dbg("VMX_HOST_ES_SEL: 0x%hu ", value16);

	asm volatile ("movw %%cs, %%ax":"=a" (value16));
	exec_vmwrite16(VMX_HOST_CS_SEL, value16);
	pr_dbg("VMX_HOST_CS_SEL: 0x%hu ", value16);

	asm volatile ("movw %%ss, %%ax":"=a" (value16));
	exec_vmwrite16(VMX_HOST_SS_SEL, value16);
	pr_dbg("VMX_HOST_SS_SEL: 0x%hu ", value16);

	asm volatile ("movw %%ds, %%ax":"=a" (value16));
	exec_vmwrite16(VMX_HOST_DS_SEL, value16);
	pr_dbg("VMX_HOST_DS_SEL: 0x%hu ", value16);

	asm volatile ("movw %%fs, %%ax":"=a" (value16));
	exec_vmwrite16(VMX_HOST_FS_SEL, value16);
	pr_dbg("VMX_HOST_FS_SEL: 0x%hu ", value16);

	asm volatile ("movw %%gs, %%ax":"=a" (value16));
	exec_vmwrite16(VMX_HOST_GS_SEL, value16);
	pr_dbg("VMX_HOST_GS_SEL: 0x%hu ", value16);

	asm volatile ("str %%ax":"=a" (tr_sel));
	exec_vmwrite16(VMX_HOST_TR_SEL, tr_sel);
	pr_dbg("VMX_HOST_TR_SEL: 0x%hx ", tr_sel);

	/******************************************************
	 * 32-bit fields
	 * Set up the 32 bit host state fields - pg 3418 B.3.3 * Set limit for
	 * ES, CS, DD, DS, FS, GS, LDTR, Guest TR, * GDTR, and IDTR
	 ******************************************************/

	/* TODO: Should guest GDTB point to host GDTB ? */
	/* Obtain the current global descriptor table base */
	asm volatile ("sgdt %0":"=m"(gdtb)::"memory");

	if (((gdtb.base >> 47U) & 0x1UL) != 0UL) {
		gdtb.base |= 0xffff000000000000UL;
	}

	/* Set up the guest and host GDTB base fields with current GDTB base */
	exec_vmwrite(VMX_HOST_GDTR_BASE, gdtb.base);
	pr_dbg("VMX_HOST_GDTR_BASE: 0x%x ", gdtb.base);

	/* TODO: Should guest TR point to host TR ? */
	trbase = gdtb.base + tr_sel;
	if (((trbase >> 47U) & 0x1UL) != 0UL) {
		trbase |= 0xffff000000000000UL;
	}

	/* SS segment override */
	asm volatile ("mov %0,%%rax\n"
		      ".byte 0x36\n"
		      "movq (%%rax),%%rax\n":"=a" (trbase_lo):"0"(trbase)
	    );
	realtrbase = ((trbase_lo >> 16U) & (0x0ffffUL)) |
	    (((trbase_lo >> 32U) & 0x000000ffUL) << 16U) |
	    (((trbase_lo >> 56U) & 0xffUL) << 24U);

	/* SS segment override for upper32 bits of base in ia32e mode */
	asm volatile (
		      ".byte 0x36\n"
		      "movq 8(%%rax),%%rax\n":"=a" (trbase_hi):"0"(trbase));
	realtrbase = realtrbase | (trbase_hi << 32U);

	/* Set up host and guest TR base fields */
	exec_vmwrite(VMX_HOST_TR_BASE, realtrbase);
	pr_dbg("VMX_HOST_TR_BASE: 0x%016llx ", realtrbase);

	/* Obtain the current interrupt descriptor table base */
	asm volatile ("sidt %0":"=m"(idtb)::"memory");
	/* base */
	if (((idtb.base >> 47U) & 0x1UL) != 0UL) {
		idtb.base |= 0xffff000000000000UL;
	}

	exec_vmwrite(VMX_HOST_IDTR_BASE, idtb.base);
	pr_dbg("VMX_HOST_IDTR_BASE: 0x%x ", idtb.base);

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
	vmx_msr_high = (uint32_t)(vmx_msr >> 32);
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

static void init_exec_ctrl(struct vcpu *vcpu)
{
	uint32_t value32;
	uint64_t value64;
	struct vm *vm = vcpu->vm;

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

	if (vcpu->arch_vcpu.vpid != 0U) {
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

	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS2: 0x%x ", value32);

	/*APIC-v, config APIC-access address*/
	value64 = vlapic_apicv_get_apic_access_addr(vcpu->vm);
	exec_vmwrite64(VMX_APIC_ACCESS_ADDR_FULL, value64);

	/*APIC-v, config APIC virtualized page address*/
	value64 = vlapic_apicv_get_apic_page_addr(vcpu->arch_vcpu.vlapic);
	exec_vmwrite64(VMX_VIRTUAL_APIC_PAGE_ADDR_FULL, value64);

	if (is_apicv_intr_delivery_supported()) {
		/* Disable all EOI VMEXIT by default and
		 * clear RVI and SVI.
		 */
		exec_vmwrite64(VMX_EOI_EXIT0_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT1_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT2_FULL, 0UL);
		exec_vmwrite64(VMX_EOI_EXIT3_FULL, 0UL);

		exec_vmwrite16(VMX_GUEST_INTR_STATUS, 0);
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
	value64 = hva2hpa(vm->arch_vm.iobitmap[0]);
	exec_vmwrite64(VMX_IO_BITMAP_A_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_A: 0x%016llx ", value64);
	value64 = hva2hpa(vm->arch_vm.iobitmap[1]);
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

	init_cr0_cr4_host_mask(vcpu);

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
	exec_vmwrite32(VMX_ENTRY_MSR_LOAD_COUNT, 0U);

	/* Set up VM entry interrupt information field pg 2909 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, 0U);

	/* Set up VM entry exception error code - pg 2910 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE, 0U);

	/* Set up VM entry instruction length - pg 2910 24.8.3 */
	exec_vmwrite32(VMX_ENTRY_INSTR_LENGTH, 0U);
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
	exec_vmwrite32(VMX_EXIT_MSR_STORE_COUNT, 0U);
	exec_vmwrite32(VMX_EXIT_MSR_LOAD_COUNT, 0U);
}

/**
 * @pre vcpu != NULL
 */
void init_vmcs(struct vcpu *vcpu)
{
	uint64_t vmx_rev_id;
	uint64_t vmcs_pa;

	/* Log message */
	pr_dbg("Initializing VMCS");

	/* Obtain the VM Rev ID from HW and populate VMCS page with it */
	vmx_rev_id = msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s(vcpu->arch_vcpu.vmcs, 4U, (void *)&vmx_rev_id, 4U);

	/* Execute VMCLEAR on current VMCS */
	vmcs_pa = hva2hpa(vcpu->arch_vcpu.vmcs);
	exec_vmclear((void *)&vmcs_pa);

	/* Load VMCS pointer */
	exec_vmptrld((void *)&vmcs_pa);

	/* Initialize the Virtual Machine Control Structure (VMCS) */
	init_host_state(vcpu);
	/* init exec_ctrl needs to run before init_guest_state */
	init_exec_ctrl(vcpu);
	init_guest_state(vcpu);
	init_entry_ctrl(vcpu);
	init_exit_ctrl(vcpu);
}
