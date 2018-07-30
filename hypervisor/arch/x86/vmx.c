/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#ifdef CONFIG_EFI_STUB
#include <acrn_efi.h>
extern struct efi_ctx* efi_ctx;
#endif

#define REAL_MODE_BSP_INIT_CODE_SEL	(0xf000U)
#define REAL_MODE_DATA_SEG_AR		(0x0093U)
#define REAL_MODE_CODE_SEG_AR		(0x009fU)
#define PROTECTED_MODE_DATA_SEG_AR	(0xc093U)
#define PROTECTED_MODE_CODE_SEG_AR	(0xc09bU)

static uint64_t cr0_host_mask;
static uint64_t cr0_always_on_mask;
static uint64_t cr0_always_off_mask;
static uint64_t cr4_host_mask;
static uint64_t cr4_always_on_mask;
static uint64_t cr4_always_off_mask;

static inline int exec_vmxon(void *addr)
{
	uint64_t rflags;
	uint64_t tmp64;
	int status = 0;

	if (addr == NULL) {
		pr_err("%s, Incorrect arguments\n", __func__);
		return -EINVAL;
	}

	/* Read Feature ControL MSR */
	tmp64 = msr_read(MSR_IA32_FEATURE_CONTROL);

	/* Determine if feature control is locked */
	if ((tmp64 & MSR_IA32_FEATURE_CONTROL_LOCK) != 0U) {
		/* See if VMX enabled */
		if ((tmp64 & MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX) == 0U) {
			/* Return error - VMX can't be enabled */
			pr_err("%s, VMX can't be enabled\n", __func__);
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
		asm volatile (
			      "vmxon (%%rax)\n"
			      "pushfq\n"
			      "pop %0\n":"=r" (rflags)
			      : "a"(addr)
			      : "cc", "memory");

		/* if carry and zero flags are clear operation success */
		if ((rflags & (RFLAGS_C | RFLAGS_Z)) != 0U) {
			pr_err("%s, Turn VMX on failed\n", __func__);
			status = -EINVAL;
		}
	}

	/* Return result to caller */
	return status;
}

/* Per cpu data to hold the vmxon_region_pa for each pcpu.
 * It will be used again when we start a pcpu after the pcpu was down.
 * S3 enter/exit will use it.
 */
int exec_vmxon_instr(uint16_t pcpu_id)
{
	uint64_t tmp64, vmcs_pa;
	uint32_t tmp32;
	int ret = -ENOMEM;
	void *vmxon_region_va;
	struct vcpu *vcpu = get_ever_run_vcpu(pcpu_id);

	/* Allocate page aligned memory for VMXON region */
	if (per_cpu(vmxon_region_pa, pcpu_id) == 0UL) {
		vmxon_region_va = alloc_page();
	}
	else {
		vmxon_region_va = HPA2HVA(per_cpu(vmxon_region_pa, pcpu_id));
	}

	if (vmxon_region_va != NULL) {
		/* Initialize vmxon page with revision id from IA32 VMX BASIC
		 * MSR
		 */
		tmp32 = (uint32_t)msr_read(MSR_IA32_VMX_BASIC);
		(void)memcpy_s((uint32_t *) vmxon_region_va, 4U, (void *)&tmp32, 4U);

		/* Turn on CR0.NE and CR4.VMXE */
		CPU_CR_READ(cr0, &tmp64);
		CPU_CR_WRITE(cr0, tmp64 | CR0_NE);
		CPU_CR_READ(cr4, &tmp64);
		CPU_CR_WRITE(cr4, tmp64 | CR4_VMXE);

		/* Turn ON VMX */
		per_cpu(vmxon_region_pa, pcpu_id) = HVA2HPA(vmxon_region_va);
		ret = exec_vmxon(&per_cpu(vmxon_region_pa, pcpu_id));

		if (vcpu != NULL) {
			vmcs_pa = HVA2HPA(vcpu->arch_vcpu.vmcs);
			ret = exec_vmptrld(&vmcs_pa);
		}
	} else {
		pr_err("%s, alloc memory for VMXON region failed\n",
				__func__);
	}

	return ret;
}

int vmx_off(uint16_t pcpu_id)
{
	int ret = 0;

	struct vcpu *vcpu = get_ever_run_vcpu(pcpu_id);
	uint64_t vmcs_pa;

	if (vcpu != NULL) {
		vmcs_pa = HVA2HPA(vcpu->arch_vcpu.vmcs);
		ret = exec_vmclear((void *)&vmcs_pa);
		if (ret != 0) {
			return ret;
		}
	}

	asm volatile ("vmxoff" : : : "memory");

	return 0;
}

int exec_vmclear(void *addr)
{
	uint64_t rflags;
	int status = 0;

	if (addr == NULL) {
		status = -EINVAL;
	}
	ASSERT(status == 0, "Incorrect arguments");

	asm volatile (
		"vmclear (%%rax)\n"
		"pushfq\n"
		"pop %0\n"
		:"=r" (rflags)
		: "a"(addr)
		: "cc", "memory");

	/* if carry and zero flags are clear operation success */
	if ((rflags & (RFLAGS_C | RFLAGS_Z)) != 0U) {
		status = -EINVAL;
	}

	return status;
}

int exec_vmptrld(void *addr)
{
	uint64_t rflags;
	int status = 0;

	if (addr == NULL) {
		status = -EINVAL;
	}
	ASSERT(status == 0, "Incorrect arguments");

	asm volatile (
		"vmptrld (%%rax)\n"
		"pushfq\n"
		"pop %0\n"
		: "=r" (rflags)
		: "a"(addr)
		: "cc", "memory");

	/* if carry and zero flags are clear operation success */
	if ((rflags & (RFLAGS_C | RFLAGS_Z)) != 0U) {
		status = -EINVAL;
	}

	return status;
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

#define HV_ARCH_VMX_GET_CS(SEL)				\
{							\
	asm volatile ("movw %%cs, %%ax" : "=a"(sel));	\
}

static uint32_t get_cs_access_rights(void)
{
	uint32_t usable_ar;
	uint16_t sel_value;

	asm volatile ("movw %%cs, %%ax" : "=a" (sel_value));
	asm volatile ("lar %%eax, %%eax" : "=a" (usable_ar) : "a"(sel_value));
	usable_ar = usable_ar >> 8U;
	usable_ar &= 0xf0ffU;	/* clear bits 11:8 */

	return usable_ar;
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
	struct run_context *context =
			&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	/*
	 * note: if context->cr0.CD is set, the actual value in guest's
	 * IA32_PAT MSR is PAT_ALL_UC_VALUE, which may be different from
	 * the saved value context->ia32_pat
	 */
	return context->ia32_pat;
}

int vmx_wrmsr_pat(struct vcpu *vcpu, uint64_t value)
{
	uint32_t i;
	uint64_t field;
	struct run_context *context =
			 &vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	for (i = 0U; i < 8U; i++) {
		field = (value >> (i * 8U)) & 0xffUL;
		if ((PAT_MEM_TYPE_INVALID(field) ||
				(PAT_FIELD_RSV_BITS & field) != 0UL)) {
			pr_err("invalid guest IA32_PAT: 0x%016llx", value);
			vcpu_inject_gp(vcpu, 0U);
			return 0;
		}
	}

	context->ia32_pat = value;

	/*
	 * If context->cr0.CD is set, we defer any further requests to write
	 * guest's IA32_PAT, until the time when guest's CR0.CD is being cleared
	 */
	if ((context->cr0 & CR0_CD) == 0UL) {
		exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value);
	}
	return 0;
}

static bool is_cr0_write_valid(struct vcpu *vcpu, uint64_t cr0)
{
	struct run_context *context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

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
	if (((cr0 & CR0_PG) != 0UL) && ((context->cr4 & CR4_PAE) == 0UL) &&
			((context->ia32_efer & MSR_IA32_EFER_LME_BIT) != 0UL))
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
int vmx_write_cr0(struct vcpu *vcpu, uint64_t cr0)
{
	struct run_context *context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	uint64_t cr0_vmx;
	uint32_t entry_ctrls;
	bool paging_enabled = !!(context->cr0 & CR0_PG);

	if (!is_cr0_write_valid(vcpu, cr0)) {
		pr_dbg("Invalid cr0 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
		return 0;
	}

	/* SDM 2.5
	 * When loading a control register, reserved bit should always set
	 * to the value previously read.
	 */
	cr0 = (cr0 & ~CR0_RESERVED_MASK) | (context->cr0 & CR0_RESERVED_MASK);

	if (((context->ia32_efer & MSR_IA32_EFER_LME_BIT) != 0UL) &&
	    !paging_enabled && ((cr0 & CR0_PG) != 0UL)) {
		/* Enable long mode */
		pr_dbg("VMM: Enable long mode");
		entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
		entry_ctrls |= VMX_ENTRY_CTLS_IA32E_MODE;
		exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

		context->ia32_efer |= MSR_IA32_EFER_LMA_BIT;
		exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, context->ia32_efer);
	} else if (((context->ia32_efer & MSR_IA32_EFER_LME_BIT) != 0UL) &&
		   paging_enabled && ((cr0 & CR0_PG) == 0UL)){
		/* Disable long mode */
		pr_dbg("VMM: Disable long mode");
		entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
		entry_ctrls &= ~VMX_ENTRY_CTLS_IA32E_MODE;
		exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

		context->ia32_efer &= ~MSR_IA32_EFER_LMA_BIT;
		exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, context->ia32_efer);
	} else {
		/* CR0.PG unchanged. */
	}

	/* If CR0.CD or CR0.NW get changed */
	if (((context->cr0 ^ cr0) & (CR0_CD | CR0_NW)) != 0UL) {
		/* No action if only CR0.NW is changed */
		if (((context->cr0 ^ cr0) & CR0_CD) != 0UL) {
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
				exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, context->ia32_pat);
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
	context->cr0 = cr0;

	pr_dbg("VMM: Try to write %016llx, allow to write 0x%016llx to CR0",
		cr0, cr0_vmx);

	return 0;
}

int vmx_write_cr3(struct vcpu *vcpu, uint64_t cr3)
{
	struct run_context *context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	/* Write to guest's CR3 */
	context->cr3 = cr3;

	/* Commit new value to VMCS */
	exec_vmwrite(VMX_GUEST_CR3, cr3);

	return 0;
}

static bool is_cr4_write_valid(struct vcpu *vcpu, uint64_t cr4)
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
int vmx_write_cr4(struct vcpu *vcpu, uint64_t cr4)
{
	struct run_context *context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	uint64_t cr4_vmx;

	if (!is_cr4_write_valid(vcpu, cr4)) {
		pr_dbg("Invalid cr4 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
		return 0;
	}

	/* Aways off bits and reserved bits has been filtered above */
	cr4_vmx = cr4_always_on_mask | cr4;
	exec_vmwrite(VMX_GUEST_CR4, cr4_vmx & 0xFFFFFFFFUL);
	exec_vmwrite(VMX_CR4_READ_SHADOW, cr4 & 0xFFFFFFFFUL);
	context->cr4 = cr4;

	pr_dbg("VMM: Try to write %016llx, allow to write 0x%016llx to CR4",
		cr4, cr4_vmx);

	return 0;
}

static void init_guest_state(struct vcpu *vcpu)
{
	uint32_t field;
	uint64_t value;
	uint16_t value16;
	uint32_t value32;
	uint64_t value64;
	uint16_t sel;
	uint32_t limit, access;
	uint64_t base;
	uint16_t ldt_idx = 0x38U;
	uint16_t es = 0U, ss = 0U, ds = 0U, fs = 0U, gs = 0U, data32_idx;
	uint16_t tr_sel = 0x70U;
	struct vm *vm = vcpu->vm;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	enum vm_cpu_mode vcpu_mode = get_vcpu_mode(vcpu);

	pr_dbg("*********************");
	pr_dbg("Initialize guest state");
	pr_dbg("*********************");


	/* Will not init vcpu mode to compatibility mode */
	ASSERT(vcpu_mode != CPU_MODE_COMPATIBILITY,
		"don't support start vcpu from compatibility mode");

	/*************************************************/
	/* Set up CRx                                    */
	/*************************************************/
	pr_dbg("Natural-width********");

	if (vcpu_mode == CPU_MODE_64BIT) {
		cur_context->ia32_efer = MSR_IA32_EFER_LME_BIT;
	}

	/* Setup guest control register values
	 * cr4 should be set before cr0, because when set cr0, cr4 value will be
	 * checked.
	 */
	if (vcpu_mode == CPU_MODE_REAL) {
		vmx_write_cr4(vcpu, 0UL);
		vmx_write_cr3(vcpu, 0UL);
		vmx_write_cr0(vcpu, CR0_ET | CR0_NE);
	} else if (vcpu_mode == CPU_MODE_PROTECTED) {
		vmx_write_cr4(vcpu, 0UL);
		vmx_write_cr3(vcpu, 0UL);
		vmx_write_cr0(vcpu, CR0_ET | CR0_NE | CR0_PE);
	} else if (vcpu_mode == CPU_MODE_64BIT) {
		vmx_write_cr4(vcpu, CR4_PSE | CR4_PAE | CR4_MCE);
		vmx_write_cr3(vcpu, vm->arch_vm.guest_init_pml4 | CR3_PWT);
		vmx_write_cr0(vcpu, CR0_PG | CR0_PE | CR0_NE);
	} else {
		/* vcpu_mode will never be CPU_MODE_COMPATIBILITY */
	}

	/***************************************************/
	/* Set up Flags - the value of RFLAGS on VM entry */
	/***************************************************/
	field = VMX_GUEST_RFLAGS;
	cur_context->rflags = 0x2UL; 	/* Bit 1 is a active high reserved bit */
	exec_vmwrite(field, cur_context->rflags);
	pr_dbg("VMX_GUEST_RFLAGS: 0x%016llx ", cur_context->rflags);

	/***************************************************/
	/* Set Code Segment - CS */
	/***************************************************/
	if (vcpu_mode == CPU_MODE_REAL) {
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
				sel = (uint16_t)(((uint64_t)vcpu->entry_addr & 0xFFFF0UL)
					>> 4U);
				base = (uint64_t)sel << 4U;
			} else {
				/* BSP is initialized with real mode */
				sel = REAL_MODE_BSP_INIT_CODE_SEL;
				/* For unrestricted guest, it is able
				 * to set a high base address
				 */
				base = (uint64_t)vcpu->entry_addr &
					0xFFFF0000UL;
			}
		} else {
			/* AP is initialized with real mode
			 * and CS value is left shift 8 bits from sipi vector.
			 */
			sel = (uint16_t)(vcpu->arch_vcpu.sipi_vector << 8U);
			base = (uint64_t)sel << 4U;
		}
		limit = 0xffffU;
		access = REAL_MODE_CODE_SEG_AR;
	} else if (vcpu_mode == CPU_MODE_PROTECTED) {
		limit = 0xffffffffU;
		base = 0UL;
		access = PROTECTED_MODE_CODE_SEG_AR;
		sel = 0x10U;	/* Linear CS selector in guest init gdt */
	} else {
		HV_ARCH_VMX_GET_CS(sel);
		access = get_cs_access_rights();
		limit = 0xffffffffU;
		base = 0UL;
	}

	/* Selector */
	field = VMX_GUEST_CS_SEL;
	exec_vmwrite16(field, sel);
	pr_dbg("VMX_GUEST_CS_SEL: 0x%hx ", sel);

	/* Limit */
	field = VMX_GUEST_CS_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_CS_LIMIT: 0x%x ", limit);

	/* Access */
	field = VMX_GUEST_CS_ATTR;
	exec_vmwrite32(field, access);
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
	value64 = 0UL;
	if (vcpu_mode == CPU_MODE_REAL) {
		/* RIP is set here */
		if (is_vcpu_bsp(vcpu)) {
			if ((uint64_t)vcpu->entry_addr < 0x100000UL) {
				value64 = (uint64_t)vcpu->entry_addr & 0x0FUL;
			}
			else {
				value64 = 0x0000FFF0UL;
			}
		}
	} else {
		value64 = (uint64_t)vcpu->entry_addr;
	}

	pr_dbg("GUEST RIP on VMEntry %016llx ", value64);
	exec_vmwrite(field, value64);

	if (vcpu_mode == CPU_MODE_64BIT) {
		/* Set up guest stack pointer to 0 */
		field = VMX_GUEST_RSP;
		value64 = 0UL;
		pr_dbg("GUEST RSP on VMEntry %016llx ",
				value64);
		exec_vmwrite(field, value64);
	}

	/***************************************************/
	/* Set up GDTR, IDTR and LDTR */
	/***************************************************/

	/* GDTR - Global Descriptor Table */
	if (vcpu_mode == CPU_MODE_REAL) {
		/* Base */
		base = 0UL;

		/* Limit */
		limit = 0xFFFFU;
	} else if (vcpu_mode == CPU_MODE_PROTECTED) {
		base = create_guest_init_gdt(vcpu->vm, &limit);
	} else if (vcpu_mode == CPU_MODE_64BIT) {
		descriptor_table gdtb = {0U, 0UL};

		/* Base *//* TODO: Should guest GDTB point to host GDTB ? */
		/* Obtain the current global descriptor table base */
		asm volatile ("sgdt %0" : "=m"(gdtb)::"memory");

		value32 = gdtb.limit;

		if (((gdtb.base >> 47U) & 0x1UL) != 0UL) {
			gdtb.base |= 0xffff000000000000UL;
		}

		base = gdtb.base;

		/* Limit */
		limit = HOST_GDT_SIZE - 1U;
	} else {
		/* vcpu_mode will never be CPU_MODE_COMPATIBILITY */
	}

	/* GDTR Base */
	field = VMX_GUEST_GDTR_BASE;
	exec_vmwrite(field, base);
	pr_dbg("VMX_GUEST_GDTR_BASE: 0x%016llx ", base);

	/* GDTR Limit */
	field = VMX_GUEST_GDTR_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%x ", limit);

	/* IDTR - Interrupt Descriptor Table */
	if ((vcpu_mode == CPU_MODE_REAL) ||
	    (vcpu_mode == CPU_MODE_PROTECTED)) {
		/* Base */
		base = 0UL;

		/* Limit */
		limit = 0xFFFFU;
	} else if (vcpu_mode == CPU_MODE_64BIT) {
		descriptor_table idtb = {0U, 0UL};

		/* TODO: Should guest IDTR point to host IDTR ? */
		asm volatile ("sidt %0":"=m"(idtb)::"memory");
		/* Limit */
		limit = idtb.limit;

		if (((idtb.base >> 47U) & 0x1UL) != 0UL) {
			idtb.base |= 0xffff000000000000UL;
		}

		/* Base */
		base = idtb.base;
	} else {
		/* vcpu_mode will never be CPU_MODE_COMPATIBILITY */
	}

	/* IDTR Base */
	field = VMX_GUEST_IDTR_BASE;
	exec_vmwrite(field, base);
	pr_dbg("VMX_GUEST_IDTR_BASE: 0x%016llx ", base);

	/* IDTR Limit */
	field = VMX_GUEST_IDTR_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%x ", limit);

	/***************************************************/
	/* Debug register */
	/***************************************************/
	/* Set up guest Debug register */
	field = VMX_GUEST_DR7;
	value64 = 0x400UL;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_DR7: 0x%016llx ", value64);

	/***************************************************/
	/* ES, CS, SS, DS, FS, GS */
	/***************************************************/
	data32_idx = 0x10U;
	if (vcpu_mode == CPU_MODE_REAL) {
		es = data32_idx;
		ss = data32_idx;
		ds = data32_idx;
		fs = data32_idx;
		gs = data32_idx;
		limit = 0xffffU;

	} else if (vcpu_mode == CPU_MODE_PROTECTED) {
		/* Linear data segment in guest init gdt */
		es = 0x18U;
		ss = 0x18U;
		ds = 0x18U;
		fs = 0x18U;
		gs = 0x18U;
		limit = 0xffffffffU;
	} else if (vcpu_mode == CPU_MODE_64BIT) {
		asm volatile ("movw %%es, %%ax":"=a" (es));
		asm volatile ("movw %%ss, %%ax":"=a" (ss));
		asm volatile ("movw %%ds, %%ax":"=a" (ds));
		asm volatile ("movw %%fs, %%ax":"=a" (fs));
		asm volatile ("movw %%gs, %%ax":"=a" (gs));
		limit = 0xffffffffU;
	} else {
		/* vcpu_mode will never be CPU_MODE_COMPATIBILITY */
	}

	/* Selector */
	field = VMX_GUEST_ES_SEL;
	exec_vmwrite16(field, es);
	pr_dbg("VMX_GUEST_ES_SEL: 0x%hx ", es);

	field = VMX_GUEST_SS_SEL;
	exec_vmwrite16(field, ss);
	pr_dbg("VMX_GUEST_SS_SEL: 0x%hx ", ss);

	field = VMX_GUEST_DS_SEL;
	exec_vmwrite16(field, ds);
	pr_dbg("VMX_GUEST_DS_SEL: 0x%hx ", ds);

	field = VMX_GUEST_FS_SEL;
	exec_vmwrite16(field, fs);
	pr_dbg("VMX_GUEST_FS_SEL: 0x%hx ", fs);

	field = VMX_GUEST_GS_SEL;
	exec_vmwrite16(field, gs);
	pr_dbg("VMX_GUEST_GS_SEL: 0x%hx ", gs);

	/* Limit */
	field = VMX_GUEST_ES_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_ES_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_SS_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_SS_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_DS_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_DS_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_FS_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_FS_LIMIT: 0x%x ", limit);
	field = VMX_GUEST_GS_LIMIT;
	exec_vmwrite32(field, limit);
	pr_dbg("VMX_GUEST_GS_LIMIT: 0x%x ", limit);

	/* Access */
	if (vcpu_mode == CPU_MODE_REAL) {
		value32 = REAL_MODE_DATA_SEG_AR;
	}
	else {	/* same value for protected mode and long mode */
		value32 = PROTECTED_MODE_DATA_SEG_AR;
	}

	field = VMX_GUEST_ES_ATTR;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_ES_ATTR: 0x%x ", value32);
	field = VMX_GUEST_SS_ATTR;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_SS_ATTR: 0x%x ", value32);
	field = VMX_GUEST_DS_ATTR;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_DS_ATTR: 0x%x ", value32);
	field = VMX_GUEST_FS_ATTR;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_FS_ATTR: 0x%x ", value32);
	field = VMX_GUEST_GS_ATTR;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_GS_ATTR: 0x%x ", value32);

	/* Base */
	if (vcpu_mode == CPU_MODE_REAL) {
		value64 = (uint64_t)es << 4U;
	} else {
		value64 = 0UL;
	}

	field = VMX_GUEST_ES_BASE;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_ES_BASE: 0x%016llx ", value64);
	field = VMX_GUEST_SS_BASE;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_SS_BASE: 0x%016llx ", value64);
	field = VMX_GUEST_DS_BASE;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_DS_BASE: 0x%016llx ", value64);
	field = VMX_GUEST_FS_BASE;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_FS_BASE: 0x%016llx ", value64);
	field = VMX_GUEST_GS_BASE;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_GS_BASE: 0x%016llx ", value64);

	/***************************************************/
	/* LDT and TR (dummy) */
	/***************************************************/
	field = VMX_GUEST_LDTR_SEL;
	value16 = ldt_idx;
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_GUEST_LDTR_SEL: 0x%hu ", value16);

	field = VMX_GUEST_LDTR_LIMIT;
	value32 = 0xffffffffU;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_LDTR_LIMIT: 0x%x ", value32);

	field = VMX_GUEST_LDTR_ATTR;
	value32 = 0x10000U;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_LDTR_ATTR: 0x%x ", value32);

	field = VMX_GUEST_LDTR_BASE;
	value64 = 0x00UL;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_LDTR_BASE: 0x%016llx ", value64);

	/* Task Register */
	field = VMX_GUEST_TR_SEL;
	value16 = tr_sel;
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_GUEST_TR_SEL: 0x%hu ", value16);

	field = VMX_GUEST_TR_LIMIT;
	value32 = 0xffU;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_TR_LIMIT: 0x%x ", value32);

	field = VMX_GUEST_TR_ATTR;
	value32 = 0x8bU;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_TR_ATTR: 0x%x ", value32);

	field = VMX_GUEST_TR_BASE;
	value64 = 0x00UL;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_TR_BASE: 0x%016llx ", value64);

	field = VMX_GUEST_INTERRUPTIBILITY_INFO;
	value32 = 0U;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_INTERRUPTIBILITY_INFO: 0x%x ",
		  value32);

	field = VMX_GUEST_ACTIVITY_STATE;
	value32 = 0U;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_ACTIVITY_STATE: 0x%x ",
		  value32);

	field = VMX_GUEST_SMBASE;
	value32 = 0U;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_SMBASE: 0x%x ", value32);

	value32 = ((uint32_t)msr_read(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFFU);
	field = VMX_GUEST_IA32_SYSENTER_CS;
	exec_vmwrite32(field, value32);
	pr_dbg("VMX_GUEST_IA32_SYSENTER_CS: 0x%x ",
		  value32);

	value64 = PAT_POWER_ON_VALUE;
	cur_context->ia32_pat = value64;
	exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, value64);
	pr_dbg("VMX_GUEST_IA32_PAT: 0x%016llx ",
		  value64);

	value64 = 0UL;
	exec_vmwrite64(VMX_GUEST_IA32_DEBUGCTL_FULL, value64);
	pr_dbg("VMX_GUEST_IA32_DEBUGCTL: 0x%016llx ",
		  value64);

	/* Set up guest pending debug exception */
	field = VMX_GUEST_PENDING_DEBUG_EXCEPT;
	value64 = 0x0UL;
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_PENDING_DEBUG_EXCEPT: 0x%016llx ", value64);

	/* These fields manage host and guest system calls * pg 3069 31.10.4.2
	 * - set up these fields with * contents of current SYSENTER ESP and
	 * EIP MSR values
	 */
	field = VMX_GUEST_IA32_SYSENTER_ESP;
	value64 = msr_read(MSR_IA32_SYSENTER_ESP);
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_IA32_SYSENTER_ESP: 0x%016llx ",
		  value64);
	field = VMX_GUEST_IA32_SYSENTER_EIP;
	value64 = msr_read(MSR_IA32_SYSENTER_EIP);
	exec_vmwrite(field, value64);
	pr_dbg("VMX_GUEST_IA32_SYSENTER_EIP: 0x%016llx ",
		  value64);
}

static void init_host_state(__unused struct vcpu *vcpu)
{
	uint32_t field;
	uint16_t value16;
	uint32_t value32;
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
	field = VMX_HOST_ES_SEL;
	asm volatile ("movw %%es, %%ax":"=a" (value16));
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_HOST_ES_SEL: 0x%hu ", value16);

	field = VMX_HOST_CS_SEL;
	asm volatile ("movw %%cs, %%ax":"=a" (value16));
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_HOST_CS_SEL: 0x%hu ", value16);

	field = VMX_HOST_SS_SEL;
	asm volatile ("movw %%ss, %%ax":"=a" (value16));
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_HOST_SS_SEL: 0x%hu ", value16);

	field = VMX_HOST_DS_SEL;
	asm volatile ("movw %%ds, %%ax":"=a" (value16));
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_HOST_DS_SEL: 0x%hu ", value16);

	field = VMX_HOST_FS_SEL;
	asm volatile ("movw %%fs, %%ax":"=a" (value16));
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_HOST_FS_SEL: 0x%hu ", value16);

	field = VMX_HOST_GS_SEL;
	asm volatile ("movw %%gs, %%ax":"=a" (value16));
	exec_vmwrite16(field, value16);
	pr_dbg("VMX_HOST_GS_SEL: 0x%hu ", value16);

	field = VMX_HOST_TR_SEL;
	asm volatile ("str %%ax":"=a" (tr_sel));
	exec_vmwrite16(field, tr_sel);
	pr_dbg("VMX_HOST_TR_SEL: 0x%hx ", tr_sel);

	/******************************************************
	 * 32-bit fields
	 * Set up the 32 bit host state fields - pg 3418 B.3.3 * Set limit for
	 * ES, CS, DD, DS, FS, GS, LDTR, Guest TR, * GDTR, and IDTR
	 ******************************************************/

	/* TODO: Should guest GDTB point to host GDTB ? */
	/* Obtain the current global descriptor table base */
	asm volatile ("sgdt %0":"=m"(gdtb)::"memory");
	value32 = gdtb.limit;

	if (((gdtb.base >> 47U) & 0x1UL) != 0UL) {
		gdtb.base |= 0xffff000000000000UL;
	}

	/* Set up the guest and host GDTB base fields with current GDTB base */
	field = VMX_HOST_GDTR_BASE;
	exec_vmwrite(field, gdtb.base);
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
	field = VMX_HOST_TR_BASE;
	exec_vmwrite(field, realtrbase);
	pr_dbg("VMX_HOST_TR_BASE: 0x%016llx ", realtrbase);

	/* Obtain the current interrupt descriptor table base */
	asm volatile ("sidt %0":"=m"(idtb)::"memory");
	/* base */
	if (((idtb.base >> 47U) & 0x1UL) != 0UL) {
		idtb.base |= 0xffff000000000000UL;
	}

	field = VMX_HOST_IDTR_BASE;
	exec_vmwrite(field, idtb.base);
	pr_dbg("VMX_HOST_IDTR_BASE: 0x%x ", idtb.base);

	value32 = (uint32_t)(msr_read(MSR_IA32_SYSENTER_CS) & 0xFFFFFFFFUL);
	field = VMX_HOST_IA32_SYSENTER_CS;
	exec_vmwrite32(field, value32);
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
	field = VMX_HOST_CR0;
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_CR0: 0x%016llx ", value);

	/* Set up host CR3 field */
	CPU_CR_READ(cr3, &value);
	field = VMX_HOST_CR3;
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_CR3: 0x%016llx ", value);

	/* Set up host CR4 field */
	CPU_CR_READ(cr4, &value);
	field = VMX_HOST_CR4;
	exec_vmwrite(field, value);
	pr_dbg("VMX_HOST_CR4: 0x%016llx ", value);

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
	value64 = (uint64_t)&vm_exit;
	pr_dbg("HOST RIP on VMExit %016llx ", value64);
	exec_vmwrite(field, value64);
	pr_dbg("vm exit return address = %016llx ", value64);

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

	if (ctrl_req & ~ctrl) {
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

	if (is_vapic_supported()) {
		value32 |= VMX_PROCBASED_CTLS_TPR_SHADOW;
	} else {
		/* Add CR8 VMExit for vlapic */
		value32 |=
			(VMX_PROCBASED_CTLS_CR8_LOAD |
			VMX_PROCBASED_CTLS_CR8_STORE);
	}

	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);
	pr_dbg("VMX_PROC_VM_EXEC_CONTROLS: 0x%x ", value32);

	/* Set up secondary processor based VM execution controls - pg 2901
	 * 24.6.2. Set up for: * Enable EPT * Enable RDTSCP * Unrestricted
	 * guest (optional)
	 */
	value32 = check_vmx_ctrl(MSR_IA32_VMX_PROCBASED_CTLS2,
			VMX_PROCBASED_CTLS2_EPT |
			VMX_PROCBASED_CTLS2_RDTSCP |
			VMX_PROCBASED_CTLS2_UNRESTRICT);

	if (vcpu->arch_vcpu.vpid != 0U) {
		value32 |= VMX_PROCBASED_CTLS2_VPID;
	} else {
		value32 &= ~VMX_PROCBASED_CTLS2_VPID;
	}

	if (is_vapic_supported()) {
		value32 |= VMX_PROCBASED_CTLS2_VAPIC;

		if (is_vapic_virt_reg_supported()) {
			value32 |= VMX_PROCBASED_CTLS2_VAPIC_REGS;
		}

		if (is_vapic_intr_delivery_supported()) {
			value32 |= VMX_PROCBASED_CTLS2_VIRQ;
		}
		else {
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
	}

	if (cpu_has_cap(X86_FEATURE_OSXSAVE)) {
		exec_vmwrite64(VMX_XSS_EXITING_BITMAP_FULL, 0UL);
		value32 |= VMX_PROCBASED_CTLS2_XSVE_XRSTR;
	}

	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, value32);
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
			/* Disable all EOI VMEXIT by default and
			 * clear RVI and SVI.
			 */
			exec_vmwrite64(VMX_EOI_EXIT0_FULL, 0UL);
			exec_vmwrite64(VMX_EOI_EXIT1_FULL, 0UL);
			exec_vmwrite64(VMX_EOI_EXIT2_FULL, 0UL);
			exec_vmwrite64(VMX_EOI_EXIT3_FULL, 0UL);

			exec_vmwrite16(VMX_GUEST_INTR_STATUS, 0);
		}
	}

	/* Check for EPT support */
	if (is_ept_supported()) {
		pr_dbg("EPT is supported");
	}
	else {
		pr_err("Error: EPT is not supported");
	}

	/* Load EPTP execution control
	 * TODO: introduce API to make this data driven based
	 * on VMX_EPT_VPID_CAP
	 */
	value64 = HVA2HPA(vm->arch_vm.nworld_eptp) | (3UL << 3U) | 6UL;
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
	value64 = HVA2HPA(vm->arch_vm.iobitmap[0]);
	exec_vmwrite64(VMX_IO_BITMAP_A_FULL, value64);
	pr_dbg("VMX_IO_BITMAP_A: 0x%016llx ", value64);
	value64 = HVA2HPA(vm->arch_vm.iobitmap[1]);
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

#ifdef CONFIG_EFI_STUB
static void override_uefi_vmcs(struct vcpu *vcpu)
{
	uint32_t field;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	if (get_vcpu_mode(vcpu) == CPU_MODE_64BIT) {
		/* CR4 should be set before CR0, because when set CR0, CR4 value
		 * will be checked. */
		/* VMXE is always on bit when set CR4, and not allowed to be set
		 * from input cr4 value */
		vmx_write_cr4(vcpu, efi_ctx->cr4 & ~CR4_VMXE);
		vmx_write_cr3(vcpu, efi_ctx->cr3);
		vmx_write_cr0(vcpu, efi_ctx->cr0 | CR0_PG | CR0_PE | CR0_NE);

		/* Selector */
		field = VMX_GUEST_CS_SEL;
		exec_vmwrite16(field, efi_ctx->cs_sel);
		pr_dbg("VMX_GUEST_CS_SEL: 0x%hx ", efi_ctx->cs_sel);

		/* Access */
		field = VMX_GUEST_CS_ATTR;
		exec_vmwrite32(field, efi_ctx->cs_ar);
		pr_dbg("VMX_GUEST_CS_ATTR: 0x%x ", efi_ctx->cs_ar);

		field = VMX_GUEST_ES_SEL;
		exec_vmwrite16(field, efi_ctx->es_sel);
		pr_dbg("VMX_GUEST_ES_SEL: 0x%hx ", efi_ctx->es_sel);

		field = VMX_GUEST_SS_SEL;
		exec_vmwrite16(field, efi_ctx->ss_sel);
		pr_dbg("VMX_GUEST_SS_SEL: 0x%hx ", efi_ctx->ss_sel);

		field = VMX_GUEST_DS_SEL;
		exec_vmwrite16(field, efi_ctx->ds_sel);
		pr_dbg("VMX_GUEST_DS_SEL: 0x%hx ", efi_ctx->ds_sel);

		field = VMX_GUEST_FS_SEL;
		exec_vmwrite16(field, efi_ctx->fs_sel);
		pr_dbg("VMX_GUEST_FS_SEL: 0x%hx ", efi_ctx->fs_sel);

		field = VMX_GUEST_GS_SEL;
		exec_vmwrite16(field, efi_ctx->gs_sel);
		pr_dbg("VMX_GUEST_GS_SEL: 0x%hx ", efi_ctx->gs_sel);

		/* Base */
		field = VMX_GUEST_ES_BASE;
		exec_vmwrite(field, efi_ctx->es_sel << 4U);
		field = VMX_GUEST_SS_BASE;
		exec_vmwrite(field, efi_ctx->ss_sel << 4U);
		field = VMX_GUEST_DS_BASE;
		exec_vmwrite(field, efi_ctx->ds_sel << 4U);
		field = VMX_GUEST_FS_BASE;
		exec_vmwrite(field, efi_ctx->fs_sel << 4U);
		field = VMX_GUEST_GS_BASE;
		exec_vmwrite(field, efi_ctx->gs_sel << 4U);

		/* RSP */
		field = VMX_GUEST_RSP;
		exec_vmwrite(field, efi_ctx->rsp);
		pr_dbg("GUEST RSP on VMEntry %x ", efi_ctx->rsp);

		/* GDTR Base */
		field = VMX_GUEST_GDTR_BASE;
		exec_vmwrite(field, efi_ctx->gdt.base);
		pr_dbg("VMX_GUEST_GDTR_BASE: 0x%016llx ", efi_ctx->gdt.base);

		/* GDTR Limit */
		field = VMX_GUEST_GDTR_LIMIT;
		exec_vmwrite32(field, efi_ctx->gdt.limit);
		pr_dbg("VMX_GUEST_GDTR_LIMIT: 0x%x ", efi_ctx->gdt.limit);

		/* IDTR Base */
		field = VMX_GUEST_IDTR_BASE;
		exec_vmwrite(field, efi_ctx->idt.base);
		pr_dbg("VMX_GUEST_IDTR_BASE: 0x%016llx ", efi_ctx->idt.base);

		/* IDTR Limit */
		field = VMX_GUEST_IDTR_LIMIT;
		exec_vmwrite32(field, efi_ctx->idt.limit);
		pr_dbg("VMX_GUEST_IDTR_LIMIT: 0x%x ", efi_ctx->idt.limit);
	}

	/* Interrupt */
	field = VMX_GUEST_RFLAGS;
	/* clear flags for CF/PF/AF/ZF/SF/OF */
	cur_context->rflags = efi_ctx->rflags & ~(0x8d5UL);
	exec_vmwrite(field, cur_context->rflags);
	pr_dbg("VMX_GUEST_RFLAGS: 0x%016llx ", cur_context->rflags);
}
#endif

int init_vmcs(struct vcpu *vcpu)
{
	uint64_t vmx_rev_id;
	int status = 0;
	uint64_t vmcs_pa;

	if (vcpu == NULL) {
		status = -EINVAL;
	}
	ASSERT(status == 0, "Incorrect arguments");

	/* Log message */
	pr_dbg("Initializing VMCS");

	/* Obtain the VM Rev ID from HW and populate VMCS page with it */
	vmx_rev_id = msr_read(MSR_IA32_VMX_BASIC);
	(void)memcpy_s(vcpu->arch_vcpu.vmcs, 4U, (void *)&vmx_rev_id, 4U);

	/* Execute VMCLEAR on current VMCS */
	vmcs_pa = HVA2HPA(vcpu->arch_vcpu.vmcs);
	status = exec_vmclear((void *)&vmcs_pa);
	ASSERT(status == 0, "Failed VMCLEAR during VMCS setup!");

	/* Load VMCS pointer */
	status = exec_vmptrld((void *)&vmcs_pa);
	ASSERT(status == 0, "Failed VMCS pointer load!");

	/* Initialize the Virtual Machine Control Structure (VMCS) */
	init_host_state(vcpu);
	/* init exec_ctrl needs to run before init_guest_state */
	init_exec_ctrl(vcpu);
	init_guest_state(vcpu);
	init_entry_ctrl(vcpu);
	init_exit_ctrl(vcpu);

#ifdef CONFIG_EFI_STUB
	if (is_vm0(vcpu->vm) && vcpu->pcpu_id == BOOT_CPU_ID) {
		override_uefi_vmcs(vcpu);
	}
#endif
	/* Return status to caller */
	return status;
}
