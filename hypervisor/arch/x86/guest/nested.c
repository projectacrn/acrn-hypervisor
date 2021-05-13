/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/nested.h>

/* The only purpose of this array is to serve the is_vmx_msr() function */
static const uint32_t vmx_msrs[NUM_VMX_MSRS] = {
	LIST_OF_VMX_MSRS
};

bool is_vmx_msr(uint32_t msr)
{
	bool found = false;
	uint32_t i;

	for (i = 0U; i < NUM_VMX_MSRS; i++) {
		if (msr == vmx_msrs[i]) {
			found = true;
			break;
		}
	}

	return found;
}

static uint64_t adjust_vmx_ctrls(uint32_t msr, uint64_t request_bits)
{
	union value_64 val64, msr_val;

	/*
	 * ISDM Appendix A.3, A.4, A.5:
	 * - Bits 31:0 indicate the allowed 0-settings of these controls.
	 *   bit X of the corresponding VM-execution controls field is allowed to be 0
	 *   if bit X in the MSR is cleared to 0
	 * - Bits 63:32 indicate the allowed 1-settings of these controls.
	 *   VM entry allows control X to be 1 if bit 32+X in the MSR is set to 1
	 */
	msr_val.full = msr_read(msr);

	/*
	 * The reserved bits in VMCS Control fields could be 0 or 1, determined by the
	 * corresponding capability MSR. So need to read them from physical MSR.
	 *
	 * We consider the bits that are set in the allowed 0-settings group as the
	 * minimal set of bits that need to be set from the physical processor's perspective.
	 * Since we shadow this control field, we passthru the allowed 0-settings bits.
	 */
	val64.u.lo_32 = msr_val.u.lo_32;

	/* allowed 1-settings include those bits are NOT allowed to be 0 */
	val64.u.hi_32 = msr_val.u.lo_32;

	/* make sure the requested features are supported by hardware */
	val64.u.hi_32 |= (msr_val.u.hi_32 & request_bits);

	return val64.full;
}

/*
 * @pre vcpu != NULL
 */
void init_vmx_msrs(struct acrn_vcpu *vcpu)
{
	union value_64 val64;
	uint64_t request_bits, msr_value;

	if (is_nvmx_configured(vcpu->vm)) {
		/* MSR_IA32_VMX_BASIC */
		val64.full = VMCS12_REVISION_ID	/* Bits 30:0 - VMCS revision ID */
			| (4096UL << 32U)	/* Bits 44:32 - size of VMXON region and VMCS region */
			| (6UL << 50U)		/* Bits 53:50 - memory type for VMCS etc. (6: Write Back) */
			| (1UL << 54U)		/* Bit 54: VM-exit instruction-information for INS and OUTS */
			| (1UL << 55U);		/* Bit 55: VMX controls that default to 1 may be cleared to 0 */
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_BASIC, val64.full);

		/* MSR_IA32_VMX_MISC */

		/*
		 * some bits need to read from physical MSR. For exmaple Bits 4:0 report the relationship between
		 * the rate of the VMX-preemption timer and that of the timestamp counter (TSC).
		 */
		val64.full = msr_read(MSR_IA32_VMX_MISC);
		val64.u.hi_32 = 0U;

		/* Don't support Intel® Processor Trace (Intel PT) in VMX operation */
		val64.u.lo_32 &= ~(1U << 14U);

		/* Don't support SMM in VMX operation */
		val64.u.lo_32 &= ~((1U << 15U) | (1U << 28U));

		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_MISC, val64.full);

		/*
		 * TODO: These emulated VMX Control MSRs work for Tiger Lake and Kaby Lake,
		 * potentially it may have problems if run on other platforms.
		 *
		 * We haven't put our best efforts to try to enable as much as features as
		 * possible.
		 */

		/* MSR_IA32_VMX_PINBASED_CTLS */
		request_bits = VMX_PINBASED_CTLS_IRQ_EXIT
			| VMX_PINBASED_CTLS_NMI_EXIT
			| VMX_PINBASED_CTLS_ENABLE_PTMR;
		msr_value = adjust_vmx_ctrls(MSR_IA32_VMX_PINBASED_CTLS, request_bits);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_TRUE_PINBASED_CTLS, msr_value);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_PINBASED_CTLS, msr_value);

		/* MSR_IA32_VMX_PROCBASED_CTLS */
		request_bits = VMX_PROCBASED_CTLS_IRQ_WIN | VMX_PROCBASED_CTLS_TSC_OFF
			| VMX_PROCBASED_CTLS_HLT | VMX_PROCBASED_CTLS_INVLPG
			| VMX_PROCBASED_CTLS_MWAIT | VMX_PROCBASED_CTLS_RDPMC
			| VMX_PROCBASED_CTLS_RDTSC | VMX_PROCBASED_CTLS_CR3_LOAD
			| VMX_PROCBASED_CTLS_CR3_STORE | VMX_PROCBASED_CTLS_CR8_LOAD
			| VMX_PROCBASED_CTLS_CR8_STORE | VMX_PROCBASED_CTLS_NMI_WINEXIT
			| VMX_PROCBASED_CTLS_MOV_DR | VMX_PROCBASED_CTLS_UNCOND_IO
			| VMX_PROCBASED_CTLS_MSR_BITMAP | VMX_PROCBASED_CTLS_MONITOR
			| VMX_PROCBASED_CTLS_PAUSE | VMX_PROCBASED_CTLS_SECONDARY;
		msr_value = adjust_vmx_ctrls(MSR_IA32_VMX_PROCBASED_CTLS, request_bits);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_PROCBASED_CTLS, msr_value);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_TRUE_PROCBASED_CTLS, msr_value);

		/* MSR_IA32_VMX_PROCBASED_CTLS2 */
		request_bits = VMX_PROCBASED_CTLS2_EPT | VMX_PROCBASED_CTLS2_RDTSCP
			| VMX_PROCBASED_CTLS2_VPID | VMX_PROCBASED_CTLS2_WBINVD
			| VMX_PROCBASED_CTLS2_UNRESTRICT | VMX_PROCBASED_CTLS2_PAUSE_LOOP
			| VMX_PROCBASED_CTLS2_RDRAND | VMX_PROCBASED_CTLS2_INVPCID
			| VMX_PROCBASED_CTLS2_RDSEED | VMX_PROCBASED_CTLS2_XSVE_XRSTR
			| VMX_PROCBASED_CTLS2_PT_USE_GPA | VMX_PROCBASED_CTLS2_TSC_SCALING;
		msr_value = adjust_vmx_ctrls(MSR_IA32_VMX_PROCBASED_CTLS2, request_bits);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_PROCBASED_CTLS2, msr_value);

		/* MSR_IA32_VMX_EXIT_CTLS */
		request_bits = VMX_EXIT_CTLS_SAVE_DBG | VMX_EXIT_CTLS_HOST_ADDR64
			| VMX_EXIT_CTLS_ACK_IRQ | VMX_EXIT_CTLS_LOAD_PAT
			| VMX_EXIT_CTLS_LOAD_EFER;
		msr_value = adjust_vmx_ctrls(MSR_IA32_VMX_EXIT_CTLS, request_bits);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_EXIT_CTLS, msr_value);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_TRUE_EXIT_CTLS, msr_value);

		/* MSR_IA32_VMX_ENTRY_CTLS */
		request_bits = VMX_ENTRY_CTLS_LOAD_DBG | VMX_ENTRY_CTLS_IA32E_MODE
			| VMX_ENTRY_CTLS_LOAD_PERF | VMX_ENTRY_CTLS_LOAD_PAT
			| VMX_ENTRY_CTLS_LOAD_EFER;
		msr_value = adjust_vmx_ctrls(MSR_IA32_VMX_ENTRY_CTLS, request_bits);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_ENTRY_CTLS, msr_value);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_TRUE_ENTRY_CTLS, msr_value);

		/* For now passthru the value from physical MSR to L1 guest */
		msr_value = msr_read(MSR_IA32_VMX_EPT_VPID_CAP);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_EPT_VPID_CAP, msr_value);

		msr_value = msr_read(MSR_IA32_VMX_CR0_FIXED0);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_CR0_FIXED0, msr_value);

		msr_value = msr_read(MSR_IA32_VMX_CR0_FIXED1);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_CR0_FIXED1, msr_value);

		msr_value = msr_read(MSR_IA32_VMX_CR4_FIXED0);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_CR4_FIXED0, msr_value);

		msr_value = msr_read(MSR_IA32_VMX_CR4_FIXED1);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_CR4_FIXED1, msr_value);

		msr_value = msr_read(MSR_IA32_VMX_VMCS_ENUM);
		vcpu_set_guest_msr(vcpu, MSR_IA32_VMX_VMCS_ENUM, msr_value);
	}
}

/*
 * @pre vcpu != NULL
 */
int32_t read_vmx_msr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val)
{
	uint64_t v = 0UL;
	int32_t err = 0;

	if (is_nvmx_configured(vcpu->vm)) {
		switch (msr) {
		case MSR_IA32_VMX_TRUE_PINBASED_CTLS:
		case MSR_IA32_VMX_PINBASED_CTLS:
		case MSR_IA32_VMX_PROCBASED_CTLS:
		case MSR_IA32_VMX_TRUE_PROCBASED_CTLS:
		case MSR_IA32_VMX_PROCBASED_CTLS2:
		case MSR_IA32_VMX_EXIT_CTLS:
		case MSR_IA32_VMX_TRUE_EXIT_CTLS:
		case MSR_IA32_VMX_ENTRY_CTLS:
		case MSR_IA32_VMX_TRUE_ENTRY_CTLS:
		case MSR_IA32_VMX_BASIC:
		case MSR_IA32_VMX_MISC:
		case MSR_IA32_VMX_EPT_VPID_CAP:
		case MSR_IA32_VMX_CR0_FIXED0:
		case MSR_IA32_VMX_CR0_FIXED1:
		case MSR_IA32_VMX_CR4_FIXED0:
		case MSR_IA32_VMX_CR4_FIXED1:
		case MSR_IA32_VMX_VMCS_ENUM:
		{
			v = vcpu_get_guest_msr(vcpu, msr);
			break;
		}
		/* Don't support these MSRs yet */
		case MSR_IA32_SMBASE:
		case MSR_IA32_VMX_PROCBASED_CTLS3:
		case MSR_IA32_VMX_VMFUNC:
		default:
			err = -EACCES;
			break;
		}
	} else {
		err = -EACCES;
	}

	*val = v;
	return err;
}
