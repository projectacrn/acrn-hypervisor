/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * this file contains vmcs operations which is vcpu related
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <asm/guest/virq.h>
#include <asm/mmu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/vmx.h>
#include <asm/vtd.h>
#include <asm/guest/vmexit.h>
#include <asm/pgtable.h>
#include <asm/cpufeatures.h>
#include <trace.h>
#include <logmsg.h>

/*
 * Physical CR4 bits in VMX operation may be either flexible or fixed.
 * Guest CR4 bits may be operatable or reserved.
 *
 * All the guest reserved bits should be TRAPed and EMULATed by HV
 * (inject #GP).
 *
 * For guest operatable bits, it may be:
 * CR4_PASSTHRU_BITS:
 *	Bits that may be passed through to guest. The actual passthru bits
 *	should be masked by flexible bits.
 *
 * CR4_TRAP_AND_PASSTHRU_BITS:
 *	The bits are trapped by HV and HV emulation will eventually write
 *	the guest value to physical CR4 (GUEST_CR4) too. The actual bits
 *	should be masked by flexible bits.
 *
 * CR4_TRAP_AND_EMULATE_BITS:
 *	The bits are trapped by HV and emulated, but HV updates vCR4 only
 *	(no update to physical CR4), i.e. pure software emulation.
 *
 * CR4_EMULATED_RESERVE_BITS:
 *	The bits are trapped, but are emulated by injecting a #GP.
 *
 * NOTE: Above bits should not overlap.
 *
 */
#define CR4_PASSTHRU_BITS	(CR4_VME | CR4_PVI | CR4_TSD | CR4_DE | \
				CR4_PGE | CR4_PCE | CR4_OSFXSR | CR4_PCIDE | \
				CR4_OSXSAVE | CR4_FSGSBASE | CR4_OSXMMEXCPT | \
				CR4_UMIP | CR4_LA57)
static uint64_t cr4_passthru_mask = CR4_PASSTHRU_BITS;	/* bound to flexible bits */

#define CR4_TRAP_AND_PASSTHRU_BITS	(CR4_PSE | CR4_PAE | CR4_SMEP | CR4_SMAP | CR4_PKE | CR4_PKS | CR4_KL)
static uint64_t	cr4_trap_and_passthru_mask = CR4_TRAP_AND_PASSTHRU_BITS; /* bound to flexible bits */

#ifdef CONFIG_NVMX_ENABLED
#define CR4_TRAP_AND_EMULATE_BITS	(CR4_VMXE | CR4_MCE) /* software emulated bits even if host is fixed */
#else
#define CR4_TRAP_AND_EMULATE_BITS	CR4_MCE /* software emulated bits even if host is fixed */
#endif

/* Change of these bits should change vcpuid too */
#ifdef CONFIG_NVMX_ENABLED
#define CR4_EMULATED_RESERVE_BITS	(CR4_CET | CR4_SMXE)
#else
#define CR4_EMULATED_RESERVE_BITS	(CR4_VMXE | CR4_CET | CR4_SMXE)
#endif

/* The physical CR4 value for bits of CR4_EMULATED_RESERVE_BITS */
#define CR4_EMRSV_BITS_PHYS_VALUE	CR4_VMXE

/* The CR4 value guest expected to see for bits of CR4_EMULATED_RESERVE_BITS */
#define CR4_EMRSV_BITS_VIRT_VALUE	0UL
static uint64_t cr4_rsv_bits_guest_value;

/*
 * Initial value or reset value of GUEST_CR4, i.e. physical value.
 * They are likely zeros, but some reserved bits may be not.
 */
static uint64_t initial_guest_cr4;

/*
 * Bits not in cr4_passthru_mask/cr4_trap_and_passthru_mask/cr4_trap_and_emulate_mask
 * are reserved bits, includes at least CR4_EMULATED_RESERVE_BITS
 */
static uint64_t cr4_reserved_bits_mask;

/*
 * CR0 follows the same rule of CR4, except it won't inject #GP for reserved bits violation
 * for the low 32 bits. Instead, it ignores the software write to those reserved bits.
 */
#define CR0_PASSTHRU_BITS	(CR0_MP | CR0_EM | CR0_TS | CR0_ET | CR0_NE | CR0_AM)
static uint64_t cr0_passthru_mask = CR0_PASSTHRU_BITS;	/* bound to flexible bits */

#define CR0_TRAP_AND_PASSTHRU_BITS	(CR0_PE | CR0_PG | CR0_WP)
static uint64_t	cr0_trap_and_passthru_mask = CR0_TRAP_AND_PASSTHRU_BITS;/* bound to flexible bits */
/* software emulated bits even if host is fixed */
#define CR0_TRAP_AND_EMULATE_BITS	(CR0_CD | CR0_NW)

/* These bits may be part of flexible bits but reserved to guest */
#define CR0_EMULATED_RESERVE_BITS	0UL
#define	CR0_EMRSV_BITS_PHYS_VALUE	0UL
#define	CR0_EMRSV_BITS_VIRT_VALUE	0UL
static uint64_t cr0_rsv_bits_guest_value;
static uint64_t initial_guest_cr0;		/* Initial value of GUEST_CR0 */
static uint64_t cr0_reserved_bits_mask;

/* PAE PDPTE bits 1 ~ 2, 5 ~ 8 are always reserved */
#define PAE_PDPTE_FIXED_RESVD_BITS	0x00000000000001E6UL

static int32_t load_pdptrs(const struct acrn_vcpu *vcpu)
{
	uint64_t guest_cr3 = exec_vmread(VMX_GUEST_CR3);
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();
	int32_t ret = 0;
	uint64_t pdpte[4]; /* Total four PDPTE */
	uint64_t rsvd_bits_mask;
	uint8_t maxphyaddr;
	int32_t	i;

	/* check whether the address area pointed by the guest cr3
	 * can be accessed or not
	 */
	if (copy_from_gpa(vcpu->vm, pdpte, get_pae_pdpt_addr(guest_cr3), sizeof(pdpte)) != 0) {
		ret = -EFAULT;
	} else {
		/* Check if any of the PDPTEs sets both the P flag
		 * and any reserved bit
		 */
		maxphyaddr = cpu_info->phys_bits;
		/* reserved bits: 1~2, 5~8, maxphyaddr ~ 63 */
		rsvd_bits_mask = (63U < maxphyaddr) ? 0UL : (((1UL << (63U - maxphyaddr + 1U)) - 1UL) << maxphyaddr);
		rsvd_bits_mask |= PAE_PDPTE_FIXED_RESVD_BITS;
		for (i = 0; i < 4; i++) {
			if (((pdpte[i] & PAGE_PRESENT) != 0UL) && ((pdpte[i] & rsvd_bits_mask) != 0UL)) {
				ret = -EFAULT;
				break;
			}
		}
	}

	if (ret == 0) {
		exec_vmwrite64(VMX_GUEST_PDPTE0_FULL, pdpte[0]);
		exec_vmwrite64(VMX_GUEST_PDPTE1_FULL, pdpte[1]);
		exec_vmwrite64(VMX_GUEST_PDPTE2_FULL, pdpte[2]);
		exec_vmwrite64(VMX_GUEST_PDPTE3_FULL, pdpte[3]);
	}

	return ret;
}

/*
 * Whether the value changes the reserved bits.
 */
static inline bool is_valid_cr0(uint64_t cr0)
{
	return (cr0 & cr0_reserved_bits_mask) == cr0_rsv_bits_guest_value;
}

/*
 * Certain combination of CR0 write may lead to #GP.
 */
static bool is_cr0_write_valid(struct acrn_vcpu *vcpu, uint64_t cr0)
{
	bool ret = true;

	/*
	 * Set 1 in high 32 bits (part of reserved bits) leads to #GP.
	 */
	if ((cr0 >> 32UL) != 0UL) {
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
				/* SDM 4.10.1 "Process-Context Identifiers"
				 *
				 * MOV to CR0 causes a general-protection exception if it would
				 * clear CR0.PG to 0 while CR4.PCIDE = 1
				 */
				if (((cr0 & CR0_PG) == 0UL) && ((vcpu_get_cr4(vcpu) & CR4_PCIDE) != 0UL)) {
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
static void vmx_write_cr0(struct acrn_vcpu *vcpu, uint64_t value)
{
	bool err_found = false;
	/*
	 * For reserved bits of CR0, SDM states:
	 * attempts to set them have no impact, while set to high 32 bits lead to #GP.
	 */

	if (!is_cr0_write_valid(vcpu, value)) {
		pr_err("Invalid cr0 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
	} else {
		uint64_t effective_cr0 = (value & ~cr0_reserved_bits_mask) | cr0_rsv_bits_guest_value;
		uint64_t mask, tmp;
		uint32_t entry_ctrls;
		uint64_t cr0_changed_bits = vcpu_get_cr0(vcpu) ^ effective_cr0;

		if ((cr0_changed_bits & CR0_PG) != 0UL) {
			/* PG bit changes */
			if ((effective_cr0 & CR0_PG) != 0UL) {
				/* Enable paging */
				if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
					/* Enable long mode */
					pr_dbg("VMM: Enable long mode");
					entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
					entry_ctrls |= VMX_ENTRY_CTLS_IA32E_MODE;
					exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

					vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) | MSR_IA32_EFER_LMA_BIT);
				} else {
					pr_dbg("VMM: NOT Enable long mode");
					if (is_pae(vcpu)) {
						/* enabled PAE from paging disabled */
						if (load_pdptrs(vcpu) != 0) {
							err_found = true;
							vcpu_inject_gp(vcpu, 0U);
						}
					}
				}
			} else  {
				/* Disable paging */
				pr_dbg("disable paginge");
				if ((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LME_BIT) != 0UL) {
					/* Disable long mode */
					pr_dbg("VMM: Disable long mode");
					entry_ctrls = exec_vmread32(VMX_ENTRY_CONTROLS);
					entry_ctrls &= ~VMX_ENTRY_CTLS_IA32E_MODE;
					exec_vmwrite32(VMX_ENTRY_CONTROLS, entry_ctrls);

					vcpu_set_efer(vcpu, vcpu_get_efer(vcpu) & ~MSR_IA32_EFER_LMA_BIT);
				}
			}
		}

		if (!err_found) {
			/* If CR0.CD or CR0.NW get cr0_changed_bits */
			if ((cr0_changed_bits & CR0_TRAP_AND_EMULATE_BITS) != 0UL) {
				/* No action if only CR0.NW is changed */
				if ((cr0_changed_bits & CR0_CD) != 0UL) {
					if ((effective_cr0 & CR0_CD) != 0UL) {
						/*
						 * When the guest requests to set CR0.CD, we don't allow
						 * guest's CR0.CD to be actually set, instead, we write guest
						 * IA32_PAT with all-UC entries to emulate the cache
						 * disabled behavior
						 */
						exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL, PAT_ALL_UC_VALUE);
					} else {
						/* Restore IA32_PAT to enable cache again */
						exec_vmwrite64(VMX_GUEST_IA32_PAT_FULL,
							vcpu_get_guest_msr(vcpu, MSR_IA32_PAT));
					}
				}
			}

			if ((cr0_changed_bits & (CR0_PG | CR0_WP | CR0_CD)) != 0UL) {
				vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
			}

			mask = cr0_trap_and_passthru_mask | cr0_passthru_mask;
			tmp = (initial_guest_cr0 & ~mask) | (effective_cr0 & mask);

			exec_vmwrite(VMX_GUEST_CR0, tmp);
			exec_vmwrite(VMX_CR0_READ_SHADOW, effective_cr0);

			/* clear read cache, next time read should from VMCS */
			bitmap_clear_non_atomic(CPU_REG_CR0, &vcpu->reg_cached);

			pr_dbg("VMM: Try to write %016lx, allow to write 0x%016lx to CR0", effective_cr0, tmp);
		}
	}
}

static inline bool is_valid_cr4(uint64_t cr4)
{
	return (cr4 & cr4_reserved_bits_mask) == cr4_rsv_bits_guest_value;
}

/*
 * TODO: Implement more comprhensive check here.
 */
bool is_valid_cr0_cr4(uint64_t cr0, uint64_t cr4)
{
	return is_valid_cr4(cr4) & is_valid_cr0(cr0);
}

static bool is_cr4_write_valid(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	bool ret = true;

	if (!is_valid_cr4(cr4) || (is_long_mode(vcpu) && ((cr4 & CR4_PAE) == 0UL))) {
		ret = false;
	}

	return ret;
}

/*
 * Handling of CR4:
 * Assume "unrestricted guest" feature is supported by vmx.
 *
 * For CR4, if a guest attempts to change the reserved bits, a #GP fault is injected.
 * This includes hardware reserved bits in VMX operation (not flexible bits),
 * and CR4_EMULATED_RESERVE_BITS, or check with cr4_reserved_bits_mask.
 */
static void vmx_write_cr4(struct acrn_vcpu *vcpu, uint64_t cr4)
{
	bool err_found = false;

	if (!is_cr4_write_valid(vcpu, cr4)) {
		pr_err("Invalid cr4 write operation from guest");
		vcpu_inject_gp(vcpu, 0U);
	} else {
		uint64_t mask, tmp;
		uint64_t cr4_changed_bits = vcpu_get_cr4(vcpu) ^ cr4;

		if ((cr4_changed_bits & CR4_TRAP_AND_PASSTHRU_BITS) != 0UL) {
			if (((cr4 & CR4_PAE) != 0UL) && (is_paging_enabled(vcpu)) && (!is_long_mode(vcpu))) {
				if (load_pdptrs(vcpu) != 0) {
					err_found = true;
					pr_dbg("Err found,cr4:0xlx,cr0:0x%lx ", cr4, vcpu_get_cr0(vcpu));
					vcpu_inject_gp(vcpu, 0U);
				}
			}
			vcpu_make_request(vcpu, ACRN_REQUEST_EPT_FLUSH);
		}

		if (!err_found && ((cr4_changed_bits & CR4_PCIDE) != 0UL)) {
			/* MOV to CR4 causes a general-protection exception (#GP) if it would change
			 * CR4.PCIDE from 0 to 1 and either IA32_EFER.LMA = 0 or CR3[11:0] != 000H
			 */
			if ((cr4 & CR4_PCIDE) != 0UL) {
				uint64_t guest_cr3 = exec_vmread(VMX_GUEST_CR3);

				if ((!is_long_mode(vcpu)) || ((guest_cr3 & 0xFFFUL) != 0UL)) {
					pr_dbg("Failed to enable CR4.PCID, cr4:0x%lx, cr4_changed_bits:0x%lx,vcpu_cr4:0x%lx cr3:0x%lx",
						cr4, cr4_changed_bits, vcpu_get_cr4(vcpu), guest_cr3);

					err_found = true;
					vcpu_inject_gp(vcpu, 0U);
				}
			}
		}

		if (!err_found && ((cr4_changed_bits & CR4_KL) != 0UL)) {
			if ((cr4 & CR4_KL) != 0UL) {
				vcpu->arch.cr4_kl_enabled = true;
				load_iwkey(vcpu);
			} else {
				vcpu->arch.cr4_kl_enabled = false;
			}
		}

		if (!err_found) {
			/*
			 * Update the passthru bits.
			 */
			mask = cr4_trap_and_passthru_mask | cr4_passthru_mask;
			tmp = (initial_guest_cr4 & ~mask) | (cr4 & mask);

			/*
			 * For all reserved bits (including CR4_EMULATED_RESERVE_BITS), we came here because
			 * the guest is not changing them.
			 */
			exec_vmwrite(VMX_GUEST_CR4, tmp);
			exec_vmwrite(VMX_CR4_READ_SHADOW, cr4);

			/* clear read cache, next time read should from VMCS */
			bitmap_clear_non_atomic(CPU_REG_CR4, &vcpu->reg_cached);

			pr_dbg("VMM: Try to write %016lx, allow to write 0x%016lx to CR4", cr4, tmp);
		}
	}
}

void init_cr0_cr4_flexible_bits(void)
{
	uint64_t cr0_flexible_bits;
	uint64_t cr4_flexible_bits;
	uint64_t fixed0, fixed1;

	/* make sure following MACROs don't have any overlapped set bit.
	 */
	ASSERT(((CR0_PASSTHRU_BITS ^ CR0_TRAP_AND_PASSTHRU_BITS) ^ CR0_TRAP_AND_EMULATE_BITS) ==
			(CR0_PASSTHRU_BITS | CR0_TRAP_AND_PASSTHRU_BITS | CR0_TRAP_AND_EMULATE_BITS));

	ASSERT(((CR4_PASSTHRU_BITS ^ CR4_TRAP_AND_PASSTHRU_BITS) ^ CR4_TRAP_AND_EMULATE_BITS) ==
			(CR4_PASSTHRU_BITS | CR4_TRAP_AND_PASSTHRU_BITS | CR4_TRAP_AND_EMULATE_BITS));

	/* Read the CR0 fixed0 / fixed1 MSR registers */
	fixed0 = msr_read(MSR_IA32_VMX_CR0_FIXED0);
	fixed1 = msr_read(MSR_IA32_VMX_CR0_FIXED1);

	pr_dbg("%s:cr0 fixed0 = 0x%016lx, fixed1 = 0x%016lx", __func__, fixed0, fixed1);
	cr0_flexible_bits = (fixed0 ^ fixed1);
	/*
	 * HW reports fixed bits for CR0_PG & CR0_PE, but do not check the violation.
	 * ACRN needs to set them for (unrestricted) guest, and therefore view them as
	 * flexible bits.
	 */
	cr0_flexible_bits |= (CR0_PE | CR0_PG);
	cr0_passthru_mask &= cr0_flexible_bits;
	cr0_trap_and_passthru_mask &= cr0_flexible_bits;
	cr0_reserved_bits_mask = ~(cr0_passthru_mask | cr0_trap_and_passthru_mask | CR0_TRAP_AND_EMULATE_BITS);

	/*
	 * cr0_rsv_bits_guest_value should be sync with always ON bits (1 in both FIXED0/FIXED1 MSRs).
	 * Refer SDM Appendix A.7
	 */
	cr0_rsv_bits_guest_value = (fixed0 & ~cr0_flexible_bits);
	initial_guest_cr0 = (cr0_rsv_bits_guest_value & ~CR0_EMULATED_RESERVE_BITS) | CR0_EMRSV_BITS_PHYS_VALUE;
	cr0_rsv_bits_guest_value = (cr0_rsv_bits_guest_value & ~CR0_EMULATED_RESERVE_BITS) | CR0_EMRSV_BITS_VIRT_VALUE;

	pr_dbg("cr0_flexible_bits:0x%lx, cr0_passthru_mask:%lx, cr0_trap_and_passthru_mask:%lx.\n",
		cr0_flexible_bits, cr0_passthru_mask, cr0_trap_and_passthru_mask);
	pr_dbg("cr0_reserved_bits_mask:%lx, cr0_rsv_bits_guest_value:%lx, initial_guest_cr0:%lx.\n",
		cr0_reserved_bits_mask, cr0_rsv_bits_guest_value, initial_guest_cr0);

	/* Read the CR4 fixed0 / fixed1 MSR registers */
	fixed0 = msr_read(MSR_IA32_VMX_CR4_FIXED0);
	fixed1 = msr_read(MSR_IA32_VMX_CR4_FIXED1);

	pr_dbg("%s:cr4 fixed0 = 0x%016lx, fixed1 = 0x%016lx", __func__, fixed0, fixed1);
	cr4_flexible_bits = (fixed0 ^ fixed1);
	cr4_passthru_mask &= cr4_flexible_bits;
	cr4_trap_and_passthru_mask &= cr4_flexible_bits;

	/*
	 * vcpuid should always consult cr4_reserved_bits_mask when reporting capability.
	 *
	 * The guest value of reserved bits are likely identical to fixed bits, but certains
	 * exceptions may be applied, i.e. for CR4_EMULATED_RESERVE_BITS.
	 */
	cr4_reserved_bits_mask = ~(cr4_passthru_mask | cr4_trap_and_passthru_mask | CR4_TRAP_AND_EMULATE_BITS);

	/*
	 * cr4_reserved_bits_value should be sync with always ON bits (1 in both FIXED0/FIXED1 MSRs).
	 * Refer SDM Appendix A.8
	 */
	cr4_rsv_bits_guest_value = (fixed0 & ~cr4_flexible_bits);

#ifdef CONFIG_NVMX_ENABLED
	cr4_rsv_bits_guest_value &= ~CR4_VMXE;
#endif
	initial_guest_cr4 = (cr4_rsv_bits_guest_value & ~CR4_EMULATED_RESERVE_BITS) | CR4_EMRSV_BITS_PHYS_VALUE;
	cr4_rsv_bits_guest_value = (cr4_rsv_bits_guest_value & ~CR4_EMULATED_RESERVE_BITS) | CR4_EMRSV_BITS_VIRT_VALUE;

	pr_dbg("cr4_flexible_bits:0x%lx, cr4_passthru_mask:0x%lx, cr4_trap_and_passthru_mask:0x%lx.",
		cr4_flexible_bits, cr4_passthru_mask, cr4_trap_and_passthru_mask);
	pr_dbg("cr4_reserved_bits_mask:%lx, cr4_rsv_bits_guest_value:%lx, initial_guest_cr4:%lx.\n",
		cr4_reserved_bits_mask, cr4_rsv_bits_guest_value, initial_guest_cr4);
}

void init_cr0_cr4_host_guest_mask(void)
{
	/*
	 * "1" means the bit is trapped by host, and "0" means passthru to guest..
	 */
	exec_vmwrite(VMX_CR0_GUEST_HOST_MASK, ~cr0_passthru_mask); /* all bits except passthrubits are trapped */
	pr_dbg("CR0 guest-host mask value: 0x%016lx", ~cr0_passthru_mask);

	exec_vmwrite(VMX_CR4_GUEST_HOST_MASK, ~cr4_passthru_mask); /* all bits except passthru bits are trapped */
	pr_dbg("CR4 guest-host mask value: 0x%016lx", ~cr4_passthru_mask);
}

uint64_t vcpu_get_cr0(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test_and_set_non_atomic(CPU_REG_CR0, &vcpu->reg_cached) == 0) {
		ctx->cr0 = (exec_vmread(VMX_CR0_READ_SHADOW) & ~cr0_passthru_mask) |
			(exec_vmread(VMX_GUEST_CR0) & cr0_passthru_mask);
	}
	return ctx->cr0;
}

void vcpu_set_cr0(struct acrn_vcpu *vcpu, uint64_t val)
{
	pr_dbg("%s, value: 0x%016lx rip: %016lx", __func__, val, vcpu_get_rip(vcpu));
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

/* This API shall be called after vCPU is created. */
uint64_t vcpu_get_cr4(struct acrn_vcpu *vcpu)
{
	struct run_context *ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;

	if (bitmap_test_and_set_non_atomic(CPU_REG_CR4, &vcpu->reg_cached) == 0) {
		ctx->cr4 = (exec_vmread(VMX_CR4_READ_SHADOW) & ~cr4_passthru_mask) |
			(exec_vmread(VMX_GUEST_CR4) & cr4_passthru_mask);
	}
	return ctx->cr4;
}

void vcpu_set_cr4(struct acrn_vcpu *vcpu, uint64_t val)
{
	pr_dbg("%s, value: 0x%016lx rip: %016lx", __func__, val, vcpu_get_rip(vcpu));
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
	default:
		ASSERT(false, "Unhandled CR access");
		ret = -EINVAL;
		break;
	}

	TRACE_2L(TRACE_VMEXIT_CR_ACCESS, vm_exit_cr_access_type(exit_qual),
			vm_exit_cr_access_cr_num(exit_qual));

	return ret;
}

uint64_t get_cr4_reserved_bits(void)
{
	return cr4_reserved_bits_mask;
}
