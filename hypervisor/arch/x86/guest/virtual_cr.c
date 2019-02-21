/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains vmcs operations which is vcpu related
 */

#include <hypervisor.h>
#include <cpu.h>
#include <virtual_cr.h>
#include <vtd.h>

/* CR0 bits hv want to trap to track status change */
#define CR0_TRAP_MASK (CR0_PE | CR0_PG | CR0_WP | CR0_CD | CR0_NW)
#define CR0_RESERVED_MASK ~(CR0_PG | CR0_CD | CR0_NW | CR0_AM | CR0_WP | \
			   CR0_NE |  CR0_ET | CR0_TS | CR0_EM | CR0_MP | CR0_PE)

/* CR4 bits hv want to trap to track status change */
#define CR4_TRAP_MASK (CR4_PSE | CR4_PAE | CR4_VMXE | CR4_PCIDE)
#define	CR4_RESERVED_MASK ~(CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | CR4_PSE | \
				CR4_PAE | CR4_MCE | CR4_PGE | CR4_PCE |     \
				CR4_OSFXSR | CR4_PCIDE | CR4_OSXSAVE |       \
				CR4_SMEP | CR4_FSGSBASE | CR4_VMXE |         \
				CR4_OSXMMEXCPT | CR4_SMAP | CR4_PKE |        \
				CR4_SMXE | CR4_UMIP)

static uint64_t cr0_always_on_mask;
static uint64_t cr0_always_off_mask;
static uint64_t cr4_always_on_mask;
static uint64_t cr4_always_off_mask;

static void load_pdptrs(const struct acrn_vcpu *vcpu)
{
	uint64_t guest_cr3 = exec_vmread(VMX_GUEST_CR3);
	/* TODO: check whether guest cr3 is valid */
	uint64_t *guest_cr3_hva = (uint64_t *)gpa2hva(vcpu->vm, get_pae_pdpt_addr(guest_cr3));

	stac();
	exec_vmwrite64(VMX_GUEST_PDPTE0_FULL, get_pgentry(guest_cr3_hva + 0UL));
	exec_vmwrite64(VMX_GUEST_PDPTE1_FULL, get_pgentry(guest_cr3_hva + 1UL));
	exec_vmwrite64(VMX_GUEST_PDPTE2_FULL, get_pgentry(guest_cr3_hva + 2UL));
	exec_vmwrite64(VMX_GUEST_PDPTE3_FULL, get_pgentry(guest_cr3_hva + 3UL));
	clac();
}

static bool is_cr0_write_valid(struct acrn_vcpu *vcpu, uint64_t cr0)
{
	bool ret = true;

	/* Shouldn't set always off bit */
	if ((cr0 & cr0_always_off_mask) != 0UL) {
		ret = false;
	} else {
		/* SDM 25.3 "Changes to instruction behavior in VMX non-root"
		 *
		 * We always require "unrestricted guest" control enabled. So
		 *
		 * CR0.PG = 1, CR4.PAE = 0 and IA32_EFER.LME = 1 is invalid.
		 * CR0.PE = 0 and CR0.PG = 1 is invalid.
		 */
		if (((cr0 & CR0_PG) != 0UL) && (!is_pae(vcpu)) &&
			((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL)) {
			ret = false;
		} else {
			if (((cr0 & CR0_PE) == 0UL) && ((cr0 & CR0_PG) != 0UL)) {
				ret = false;
			} else {
				/* SDM 6.15 "Exception and Interrupt Refrerence" GP Exception
				 *
				 * Loading CR0 register with a set NW flag and a clear CD flag
				 * is invalid
				 */
				if (((cr0 & CR0_CD) == 0UL) && ((cr0 & CR0_NW) != 0UL)) {
					ret = false;
				}
			}
		}
	}

	return ret;
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
static void vmx_write_cr0(struct acrn_vcpu *vcpu, uint64_t cr0)
{
	if (!is_cr0_write_valid(vcpu, cr0)) {
		pr_dbg("Invalid cr0 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
	} else {
		uint64_t cr0_vmx;
		uint32_t entry_ctrls;
		bool old_paging_enabled = is_paging_enabled(vcpu);
		uint64_t cr0_changed_bits = vcpu_get_cr0(vcpu) ^ cr0;
		uint64_t cr0_mask = cr0;

		/* SDM 2.5
		 * When loading a control register, reserved bit should always set
		 * to the value previously read.
		 */
		cr0_mask &= ~CR0_RESERVED_MASK;

		if (!old_paging_enabled && ((cr0_mask & CR0_PG) != 0UL)) {
			if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
				/* Enable long mode */
				pr_dbg("VMM: Enable long mode");
				entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
				entry_ctrls |= VMX_ENTRY_CTLS_IA32E_MODE;
				exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

				vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) | MSR_IA32_EFER_LMA_BIT);
			} else if (is_pae(vcpu)) {
				/* enabled PAE from paging disabled */
				load_pdptrs(vcpu);
			} else {
				/* do nothing */
			}
		} else if (old_paging_enabled && ((cr0_mask & CR0_PG) == 0UL)) {
			if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
				/* Disable long mode */
				pr_dbg("VMM: Disable long mode");
				entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
				entry_ctrls &= ~VMX_ENTRY_CTLS_IA32E_MODE;
				exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

				vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_LMA_BIT);
			}
		} else {
			/* do nothing */
		}

		/* If CR0.CD or CR0.NW get cr0_changed_bits */
		if ((cr0_changed_bits & (CR0_CD | CR0_NW)) != 0UL) {
			/* No action if only CR0.NW is cr0_changed_bits */
			if ((cr0_changed_bits & CR0_CD) != 0UL) {
				if ((cr0_mask & CR0_CD) != 0UL) {
					/*
					 * When the guest requests to set CR0.CD, we don't allow
					 * guest's CR0.CD to be actually set, instead, we write guest
					 * IA32_PAT with all-UC entries to emulate the cache
					 * disabled behavior
					 */
					exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, PAT_ALL_UC_VALUE);
					if (!iommu_snoop_supported(vcpu->vm->iommu)) {
						cache_flush_invalidate_all();
					}
				} else {
					/* Restore IA32_PAT to enable cache again */
					exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL,
						vcpu_get_guest_msr(vcpu, MSR_IA32_PAT));
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
		cr0_vmx = cr0_always_on_mask | cr0_mask;

		/* Don't set CD or NW bit to guest */
		cr0_vmx &= ~(CR0_CD | CR0_NW);
		exec_vmwrite(VMX_GUEST_CR0, cr0_vmx & 0xFFFFFFFFUL);
		exec_vmwrite(VMX_CR0_READ_SHADOW, cr0_mask & 0xFFFFFFFFUL);

		/* clear read cache, next time read should from VMCS */
		bitmap_clear_lock(CPU_REG_CR0, &vcpu->reg_cached);

		pr_dbg("VMM: Try to write %016llx, allow to write 0x%016llx to CR0", cr0_mask, cr0_vmx);
	}
}

static bool is_cr4_write_valid(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	bool ret = true;

	/* Check if guest try to set fixed to 0 bits or reserved bits */
	if ((cr4 & cr4_always_off_mask) != 0U) {
		ret = false;
	} else {
		/* Do NOT support nested guest, nor SMX */
		if (((cr4 & CR4_VMXE) != 0UL) || ((cr4 & CR4_SMXE) != 0UL)) {
			ret = false;
		} else {
			/* Do NOT support PCID in guest */
			if ((cr4 & CR4_PCIDE) != 0UL) {
				ret = false;
			} else {
				if (is_long_mode(vcpu)) {
					if ((cr4 & CR4_PAE) == 0UL) {
						ret = false;
					}
				}
			}
		}
	}

	return ret;
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
 *   - MCE (6) Trapped to hide from guest
 *   - PGE (7) Flexible to guest
 *   - PCE (8) Flexible to guest
 *   - OSFXSR (9) Flexible to guest
 *   - OSXMMEXCPT (10) Flexible to guest
 *   - VMXE (13) Trapped to hide from guest
 *   - SMXE (14) must always be 0 => must lead to a VM exit
 *   - PCIDE (17) Trapped to hide from guest
 *   - OSXSAVE (18) Flexible to guest
 *   - XSAVE (19) Flexible to guest
 *         We always keep align with physical cpu. So it's flexible to
 *         guest
 *   - SMEP (20) Flexible to guest
 *   - SMAP (21) Flexible to guest
 *   - PKE (22) Flexible to guest
 */
static void vmx_write_cr4(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	if (!is_cr4_write_valid(vcpu, cr4)) {
		pr_dbg("Invalid cr4 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
	} else {
		uint64_t cr4_vmx, cr4_shadow;
		uint64_t old_cr4 = vcpu_get_cr4(vcpu);

		if (((cr4 ^ old_cr4) & (CR4_PGE | CR4_PSE | CR4_PAE | CR4_SMEP | CR4_SMAP | CR4_PKE)) != 0UL) {
			if (((cr4 & CR4_PAE) != 0UL) && (is_paging_enabled(vcpu)) && (!is_long_mode(vcpu))) {
				load_pdptrs(vcpu);
			}

			vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
		}

		/* Clear forced off bits */
		cr4_shadow = cr4 & ~CR4_MCE;

		cr4_vmx = cr4_always_on_mask | cr4_shadow;
		exec_vmwrite(VMX_GUEST_CR4, cr4_vmx & 0xFFFFFFFFUL);
		exec_vmwrite(VMX_CR4_READ_SHADOW, cr4_shadow & 0xFFFFFFFFUL);

		/* clear read cache, next time read should from VMCS */
		bitmap_clear_lock(CPU_REG_CR4, &vcpu->reg_cached);

		pr_dbg("VMM: Try to write %016llx, allow to write 0x%016llx to CR4", cr4, cr4_vmx);
	}
}

void init_cr0_cr4_host_mask(void)
{
	static bool inited = false;
	static uint64_t cr0_host_owned_bits, cr4_host_owned_bits;
	uint64_t fixed0, fixed1;

	if (!inited) {
		/* Read the CR0 fixed0 / fixed1 MSR registers */
		fixed0 = msr_read(MSR_IA32_VMX_CR0_FIXED0);
		fixed1 = msr_read(MSR_IA32_VMX_CR0_FIXED1);

		cr0_host_owned_bits = ~(fixed0 ^ fixed1);
		/* Add the bit hv wants to trap */
		cr0_host_owned_bits |= CR0_TRAP_MASK;
		/* CR0 clear PE/PG from always on bits due to "unrestructed guest" feature */
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

		cr4_host_owned_bits = ~(fixed0 ^ fixed1);
		/* Add the bit hv wants to trap */
		cr4_host_owned_bits |= CR4_TRAP_MASK;
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

	exec_vmwrite(VMX_CR0_GUEST_HOST_MASK, cr0_host_owned_bits);
	/* Output CR0 mask value */
	pr_dbg("CR0 guest-host mask value: 0x%016llx", cr0_host_owned_bits);


	exec_vmwrite(VMX_CR4_GUEST_HOST_MASK, cr4_host_owned_bits);
	/* Output CR4 mask value */
	pr_dbg("CR4 guest-host mask value: 0x%016llx", cr4_host_owned_bits);
}

uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu)
{
	uint64_t mask;
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test_and_set_lock(CPU_REG_CR0, &vcpu->reg_cached) == 0) {
		mask = exec_vmread(VMX_CR0_GUEST_HOST_MASK);
		ctx->cr0 = (exec_vmread(VMX_CR0_READ_SHADOW) & mask) |
			(exec_vmread(VMX_GUEST_CR0) & (~mask));
	}
	return ctx->cr0;
}

void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val)
{
	vmx_write_cr0(vcpu, val);
}

uint64_t vcpu_get_cr2(const struct acrn_vcpu *vcpu)
{
	return vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.cr2;
}

void vcpu_set_cr2(struct acrn_vcpu *vcpu, uint64_t val)
{
	vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx.cr2 = val;
}

uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu)
{
	uint64_t mask;
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test_and_set_lock(CPU_REG_CR4, &vcpu->reg_cached) == 0) {
		mask = exec_vmread(VMX_CR4_GUEST_HOST_MASK);
		ctx->cr4 = (exec_vmread(VMX_CR4_READ_SHADOW) & mask) |
			(exec_vmread(VMX_GUEST_CR4) & (~mask));
	}
	return ctx->cr4;
}

void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val)
{
	vmx_write_cr4(vcpu, val);
}

int32_t cr_access_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t reg;
	uint32_t idx;
	uint64_t exit_qual;
	int32_t ret = 0;

	exit_qual = vcpu->arch.exit_qualification;
	idx = (uint32_t)vm_exit_cr_access_reg_idx(exit_qual);

	ASSERT((idx <= 15U), "index out of range");
	reg = vcpu_get_gpreg(vcpu, idx);

	switch ((vm_exit_cr_access_type(exit_qual) << 4U) | vm_exit_cr_access_cr_num(exit_qual)) {
	case 0x00UL:
		/* mov to cr0 */
		vcpu_set_cr0(vcpu, reg);
		break;
	case 0x04UL:
		/* mov to cr4 */
		vcpu_set_cr4(vcpu, reg);
		break;
	case 0x08UL:
		/* mov to cr8 */
		/* According to SDM 6.15 "Exception and interrupt Reference":
		 *
		 * set reserved bit in CR8 causes GP to guest
		 */
		if ((reg & ~0xFUL) != 0UL) {
			pr_dbg("Invalid cr8 write operation from guest");
			vcpu_inject_gp(vcpu, 0U);
			break;
		}
		vlapic_set_cr8(vcpu_vlapic(vcpu), reg);
		break;
	case 0x18UL:
		/* mov from cr8 */
		reg = vlapic_get_cr8(vcpu_vlapic(vcpu));
		vcpu_set_gpreg(vcpu, idx, reg);
		break;
	default:
		ASSERT(false, "Unhandled CR access");
		ret = -EINVAL;
		break;
	}

	TRACE_2L(TRACE_VMEXIT_CR_ACCESS, vm_exit_cr_access_type(exit_qual),
			vm_exit_cr_access_cr_num(exit_qual));

	return ret;
}
