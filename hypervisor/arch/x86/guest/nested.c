/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/guest/virq.h>
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

void nested_vmx_result(enum VMXResult result, int error_number)
{
	uint64_t rflags = exec_vmread(VMX_GUEST_RFLAGS);

	/* ISDM: section 30.2 CONVENTIONS */
	rflags &= ~(RFLAGS_C | RFLAGS_P | RFLAGS_A | RFLAGS_Z | RFLAGS_S | RFLAGS_O);

	if (result == VMfailValid) {
		rflags |= RFLAGS_Z;
		exec_vmwrite(VMX_INSTR_ERROR, error_number);
	} else if (result == VMfailInvalid) {
		rflags |= RFLAGS_C;
	} else {
		/* VMsucceed, do nothing */
	}

	if (result != VMsucceed) {
		pr_err("VMX failed: %d/%d", result, error_number);
	}

	exec_vmwrite(VMX_GUEST_RFLAGS, rflags);
}

/**
 * @brief get the memory-address operand of a vmx instruction
 *
 * @pre vcpu != NULL
 */
static uint64_t get_vmx_memory_operand(struct acrn_vcpu *vcpu, uint32_t instr_info)
{
	uint64_t gva, gpa, seg_base = 0UL;
	uint32_t seg, err_code = 0U;
	uint64_t offset;

	/*
	 * According to ISDM 3B: Basic VM-Exit Information: For INVEPT, INVPCID, INVVPID, LGDT,
	 * LIDT, LLDT, LTR, SGDT, SIDT, SLDT, STR, VMCLEAR, VMPTRLD, VMPTRST, VMREAD, VMWRITE,
	 * VMXON, XRSTORS, and XSAVES, the exit qualification receives the value of the instruction’s
	 * displacement field, which is sign-extended to 64 bits.
	 */
	offset = vcpu->arch.exit_qualification;

	/* TODO: should we consider the cases of address size (bits 9:7 in instr_info) is 16 or 32? */

	/*
	 * refer to ISDM Vol.1-3-24 Operand addressing on how to calculate an effective address
	 * offset = base + [index * scale] + displacement
	 * address = segment_base + offset
	 */
	if (VMX_II_BASE_REG_VALID(instr_info)) {
		offset += vcpu_get_gpreg(vcpu, VMX_II_BASE_REG(instr_info));
	}

	if (VMX_II_IDX_REG_VALID(instr_info)) {
		uint64_t val64 = vcpu_get_gpreg(vcpu, VMX_II_IDX_REG(instr_info));
		offset += (val64 << VMX_II_SCALING(instr_info));
	}

	/*
	 * In 64-bit mode, the processor treats the segment base of CS, DS, ES, SS as zero,
	 * creating a linear address that is equal to the effective address.
	 * The exceptions are the FS and GS segments, whose segment registers can be used as
	 * additional base registers in some linear address calculations.
	 */
	seg = VMX_II_SEG_REG(instr_info);
	if (seg == 4U) {
		seg_base = exec_vmread(VMX_GUEST_FS_BASE);
	}

	if (seg == 5U) {
		seg_base = exec_vmread(VMX_GUEST_GS_BASE);
	}

	gva = seg_base + offset;
	(void)gva2gpa(vcpu, gva, &gpa, &err_code);

	return gpa;
}

/*
 * @pre vcpu != NULL
 */
static uint64_t get_vmptr_gpa(struct acrn_vcpu *vcpu)
{
	uint64_t gpa, vmptr;

	/* get VMX pointer, which points to the VMCS or VMXON region GPA */
	gpa = get_vmx_memory_operand(vcpu, exec_vmread(VMX_INSTR_INFO));

	/* get the address (GPA) of the VMCS for VMPTRLD/VMCLEAR, or VMXON region for VMXON */
	(void)copy_from_gpa(vcpu->vm, (void *)&vmptr, gpa, sizeof(uint64_t));

	return vmptr;
}

static bool validate_vmptr_gpa(uint64_t vmptr_gpa)
{
	/* We don't emulate CPUID.80000008H for guests, so check with physical address width */
	struct cpuinfo_x86 *cpu_info = get_pcpu_info();

	return (mem_aligned_check(vmptr_gpa, PAGE_SIZE) && ((vmptr_gpa >> cpu_info->phys_bits) == 0UL));
}

/**
 * @pre vm != NULL
 */
static bool validate_vmcs_revision_id(struct acrn_vcpu *vcpu, uint64_t vmptr_gpa)
{
	uint32_t revision_id;

	(void)copy_from_gpa(vcpu->vm, (void *)&revision_id, vmptr_gpa, sizeof(uint32_t));

	/*
	 * VMCS revision ID must equal to what reported by the emulated IA32_VMX_BASIC MSR.
	 * The MSB of VMCS12_REVISION_ID is always smaller than 31, so the following statement
	 * implicitly validates revision_id[31] as well.
	 */
	return (revision_id == VMCS12_REVISION_ID);
}

int32_t get_guest_cpl(void)
{
	/*
	 * We get CPL from SS.DPL because:
	 *
	 * CS.DPL could not equal to the CPL for conforming code segments. ISDM 5.5 PRIVILEGE LEVELS:
	 * Conforming code segments can be accessed from any privilege level that is equal to or
	 * numerically greater (less privileged) than the DPL of the conforming code segment.
	 *
	 * ISDM 24.4.1 Guest Register State: The value of the DPL field for SS is always
	 * equal to the logical processor’s current privilege level (CPL).
	 */
	uint32_t ar = exec_vmread32(VMX_GUEST_SS_ATTR);
	return ((ar >> 5) & 3);
}

static bool validate_nvmx_cr0_cr4(uint64_t cr0_4, uint64_t fixed0, uint64_t fixed1)
{
	bool valid = true;

	/* If bit X is 1 in IA32_VMX_CR0/4_FIXED0, then that bit of CR0/4 is fixed to 1 in VMX operation */
	if ((cr0_4 & fixed0) != fixed0) {
		valid = false;
	}

	/* if bit X is 0 in IA32_VMX_CR0/4_FIXED1, then that bit of CR0/4 is fixed to 0 in VMX operation */
	/* Bits 63:32 of CR0 and CR4 are reserved and must be written with zeros */
	if ((uint32_t)(~cr0_4 & ~fixed1) != (uint32_t)~fixed1) {
		valid = false;
	}

	return valid;
}

/*
 * @pre vcpu != NULL
 */
static bool validate_nvmx_cr0(struct acrn_vcpu *vcpu)
{
	return validate_nvmx_cr0_cr4(vcpu_get_cr0(vcpu), msr_read(MSR_IA32_VMX_CR0_FIXED0),
		msr_read(MSR_IA32_VMX_CR0_FIXED1));
}

/*
 * @pre vcpu != NULL
 */
static bool validate_nvmx_cr4(struct acrn_vcpu *vcpu)
{
	return validate_nvmx_cr0_cr4(vcpu_get_cr4(vcpu), msr_read(MSR_IA32_VMX_CR4_FIXED0),
		msr_read(MSR_IA32_VMX_CR4_FIXED1));
}

/*
 * @pre vcpu != NULL
 */
int32_t vmxon_vmexit_handler(struct acrn_vcpu *vcpu)
{
	const uint64_t features = MSR_IA32_FEATURE_CONTROL_LOCK | MSR_IA32_FEATURE_CONTROL_VMX_NO_SMX;
	uint32_t ar = exec_vmread32(VMX_GUEST_CS_ATTR);

	if (is_nvmx_configured(vcpu->vm)) {
		if (((vcpu_get_cr0(vcpu) & CR0_PE) == 0UL)
			|| ((vcpu_get_cr4(vcpu) & CR4_VMXE) == 0UL)
			|| ((vcpu_get_rflags(vcpu) & RFLAGS_VM) != 0U)) {
			vcpu_inject_ud(vcpu);
		} else if (((vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) == 0U)
			|| ((ar & (1U << 13U)) == 0U)) {
			/* Current ACRN doesn't support 32 bits L1 hypervisor */
			vcpu_inject_ud(vcpu);
		} else if ((get_guest_cpl() != 0)
			|| !validate_nvmx_cr0(vcpu)
			|| !validate_nvmx_cr4(vcpu)
			|| ((vcpu_get_guest_msr(vcpu, MSR_IA32_FEATURE_CONTROL) & features) != features)) {
			vcpu_inject_gp(vcpu, 0U);
		} else if (vcpu->arch.nested.vmxon == true) {
			nested_vmx_result(VMfailValid, VMXERR_VMXON_IN_VMX_ROOT_OPERATION);
		} else {
			uint64_t vmptr_gpa = get_vmptr_gpa(vcpu);

			if (!validate_vmptr_gpa(vmptr_gpa)) {
				nested_vmx_result(VMfailInvalid, 0);
			} else if (!validate_vmcs_revision_id(vcpu, vmptr_gpa)) {
				nested_vmx_result(VMfailInvalid, 0);
			} else {
				vcpu->arch.nested.vmxon = true;
				vcpu->arch.nested.vmxon_ptr = vmptr_gpa;

				nested_vmx_result(VMsucceed, 0);
			}
		}
	} else {
		vcpu_inject_ud(vcpu);
	}

	return 0;
}
