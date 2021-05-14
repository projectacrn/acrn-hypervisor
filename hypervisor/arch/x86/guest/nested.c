/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/mmu.h>
#include <asm/guest/virq.h>
#include <asm/guest/ept.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vmcs.h>
#include <asm/guest/nested.h>

/* Cache the content of MSR_IA32_VMX_BASIC */
static uint32_t vmx_basic;

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

/* make it 1 to be able to build. correctly initialize it in next patch */
#define MAX_SHADOW_VMCS_FIELDS 1
/*
 * VMCS fields included in the dual-purpose VMCS: as shadow for L1 and
 * as hardware VMCS for nested guest (L2).
 *
 * TODO: This list is for TGL and CFL machines and the fields
 * for advacned APICv features such as Posted Interrupt and Virtual
 * Interrupt Delivery are not included, as these are not available
 * on those platforms.
 *
 * Certain fields, e.g. VMX_TSC_MULTIPLIER_FULL is available only if
 * "use TSC scaling” is supported. Thus a static array may not work
 * for all platforms.
 */
static const uint32_t vmcs_shadowing_fields[MAX_SHADOW_VMCS_FIELDS] = {
};

/* to be shared by all vCPUs for all nested guests */
static uint64_t vmcs_shadowing_bitmap[PAGE_SIZE / sizeof(uint64_t)] __aligned(PAGE_SIZE);

/*
 * This is an array of offsets into a structure of type "struct acrn_vmcs12"
 * 16 offsets for a total of 16 GROUPs. 4 "field widths" by 4 "field types".
 * "Field type" is either Control, Read-Only Data, Guest State or Host State.
 * Refer to the definition of "struct acrn_vmcs12" on how the fields are
 * grouped together for these offsets to work in tandem.
 * Refer to Intel SDM Appendix B Field Encoding in VMCS for info on how
 * fields are grouped and indexed within a group.
 */
static const uint16_t vmcs12_group_offset_table[16] = {
	offsetof(struct acrn_vmcs12, vpid),		/* 16-bit Control Fields */
	offsetof(struct acrn_vmcs12, padding),		/* 16-bit Read-Only Fields */
	offsetof(struct acrn_vmcs12, guest_es),		/* 16-bit Guest-State Fields */
	offsetof(struct acrn_vmcs12, host_es),		/* 16-bit Host-State Fields */
	offsetof(struct acrn_vmcs12, io_bitmap_a),	/* 64-bit Control Fields */
	offsetof(struct acrn_vmcs12, guest_phys_addr),	/* 64-bit Read-Only Data Fields */
	offsetof(struct acrn_vmcs12, vmcs_link_ptr),	/* 64-bit Guest-State Fields */
	offsetof(struct acrn_vmcs12, host_ia32_pat),	/* 64-bit Host-State Fields */
	offsetof(struct acrn_vmcs12, pin_based_exec_ctrl),	/* 32-bit Control Fields */
	offsetof(struct acrn_vmcs12, vm_instr_error),	/* 32-bit Read-Only Data Fields */
	offsetof(struct acrn_vmcs12, guest_es_limit),	/* 32-bit Guest-State Fields */
	offsetof(struct acrn_vmcs12, host_ia32_sysenter_cs),	/* 32-bit Host-State Fields */
	offsetof(struct acrn_vmcs12, cr0_guest_host_mask),	/* Natural-width Control Fields */
	offsetof(struct acrn_vmcs12, exit_qual),		/* Natural-width Read-Only Data Fields */
	offsetof(struct acrn_vmcs12, guest_cr0),		/* Natural-width Guest-State Fields */
	offsetof(struct acrn_vmcs12, host_cr0),			/* Natural-width Host-State Fields */
};

/*
 * field_idx is the index of the field within the group.
 *
 * Access-type is 0 for all widths except for 64-bit
 * For 64-bit if Access-type is 1, offset is moved to
 * high 4 bytes of the field.
 */
#define OFFSET_INTO_VMCS12(group_idx, field_idx, width_in_bytes, access_type) \
	(vmcs12_group_offset_table[group_idx] + \
	field_idx * width_in_bytes + \
	access_type * sizeof(uint32_t))

/* Given a vmcs field, this API returns the offset into "struct acrn_vmcs12" */
static uint16_t vmcs_field_to_vmcs12_offset(uint32_t vmcs_field)
{
	/*
	 * Refer to Appendix B Field Encoding in VMCS in SDM
	 * A value of group index 0001b is not valid because there are no 16-bit
	 * Read-Only fields.
	 *
	 * TODO: check invalid VMCS field
	 */
	uint16_t group_idx = (VMX_VMCS_FIELD_WIDTH(vmcs_field) << 2U) | VMX_VMCS_FIELD_TYPE(vmcs_field);
	uint8_t field_width = VMX_VMCS_FIELD_WIDTH(vmcs_field);
	uint8_t width_in_bytes;

	if (field_width == VMX_VMCS_FIELD_WIDTH_16) {
		width_in_bytes = 2U;
	} else if (field_width == VMX_VMCS_FIELD_WIDTH_32) {
		width_in_bytes = 4U;
	} else {
		/*
		 * Natural-width or 64-bit
		 */
		width_in_bytes = 8U;
	}

	return OFFSET_INTO_VMCS12(group_idx,
		VMX_VMCS_FIELD_INDEX(vmcs_field), width_in_bytes, /* field index within the group */
		VMX_VMCS_FIELD_ACCESS_HIGH(vmcs_field));
}

/*
 * Given a vmcs field and the pointer to the vmcs12, this API returns the
 * corresponding value from the VMCS
 */
static uint64_t vmcs12_read_field(uint64_t vmcs_hva, uint32_t field)
{
	uint64_t *ptr = (uint64_t *)(vmcs_hva + vmcs_field_to_vmcs12_offset(field));
	uint64_t val64 = 0UL;

	switch (VMX_VMCS_FIELD_WIDTH(field)) {
		case VMX_VMCS_FIELD_WIDTH_16:
			val64 = *(uint16_t *)ptr;
			break;
		case VMX_VMCS_FIELD_WIDTH_32:
			val64 = *(uint32_t *)ptr;
			break;
		case VMX_VMCS_FIELD_WIDTH_64:
			if (!!VMX_VMCS_FIELD_ACCESS_HIGH(field)) {
				val64 = *(uint32_t *)ptr;
			} else {
				val64 = *ptr;
			}
			break;
		case VMX_VMCS_FIELD_WIDTH_NATURAL:
		default:
			val64 = *ptr;
			break;
	}

	return val64;
}

/*
 * Write the given VMCS field to the given vmcs12 data structure.
 */
static void vmcs12_write_field(uint64_t vmcs_hva, uint32_t field, uint64_t val64)
{
	uint64_t *ptr = (uint64_t *)(vmcs_hva + vmcs_field_to_vmcs12_offset(field));

	switch (VMX_VMCS_FIELD_WIDTH(field)) {
		case VMX_VMCS_FIELD_WIDTH_16:
			*(uint16_t *)ptr = (uint16_t)val64;
			break;
		case VMX_VMCS_FIELD_WIDTH_32:
			*(uint32_t *)ptr = (uint32_t)val64;
			break;
		case VMX_VMCS_FIELD_WIDTH_64:
			if (!!VMX_VMCS_FIELD_ACCESS_HIGH(field)) {
				*(uint32_t *)ptr = (uint32_t)val64;
			} else {
				*ptr = val64;
			}
			break;
		case VMX_VMCS_FIELD_WIDTH_NATURAL:
		default:
			*ptr = val64;
			break;
	}
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
				vcpu->arch.nested.current_vmcs12_ptr = INVALID_GPA;

				nested_vmx_result(VMsucceed, 0);
			}
		}
	} else {
		vcpu_inject_ud(vcpu);
	}

	return 0;
}

/*
 * @pre vcpu != NULL
 */
bool check_vmx_permission(struct acrn_vcpu *vcpu)
{
	bool permit = true;

	/* If this VM is not nVMX enabled, it implies that 'vmxon == false' */
	if ((vcpu->arch.nested.vmxon == false)
		|| ((vcpu_get_cr0(vcpu) & CR0_PE) == 0UL)
		|| ((vcpu_get_rflags(vcpu) & RFLAGS_VM) != 0U)) {
		/* We rely on hardware to check "IA32_EFER.LMA = 1 and CS.L = 0" */
		vcpu_inject_ud(vcpu);
		permit = false;
	} else if (get_guest_cpl() != 0) {
		vcpu_inject_gp(vcpu, 0U);
		permit = false;
	}

	return permit;
}

/*
 * @pre vcpu != NULL
 * @pre vcpu->vm != NULL
 */
int32_t vmxoff_vmexit_handler(struct acrn_vcpu *vcpu)
{
	if (check_vmx_permission(vcpu)) {
		vcpu->arch.nested.vmxon = false;
		vcpu->arch.nested.current_vmcs12_ptr = INVALID_GPA;
		(void)memset(vcpu->arch.nested.vmcs02, 0U, PAGE_SIZE);
		(void)memset(&vcpu->arch.nested.vmcs12, 0U, sizeof(struct acrn_vmcs12));

		nested_vmx_result(VMsucceed, 0);
	}

	return 0;
}

/**
 * @brief Sync shadow fields from vmcs02 to cache VMCS12
 *
 * @pre vcpu != NULL
 * @pre vmcs02 is current
 */
static void sync_vmcs02_to_vmcs12(struct acrn_vcpu *vcpu)
{
	uint64_t vmcs12 = (uint64_t)&vcpu->arch.nested.vmcs12;
	uint64_t val64;
	uint32_t idx;

	for (idx = 0; idx < MAX_SHADOW_VMCS_FIELDS; idx++) {
		val64 = exec_vmread(vmcs_shadowing_fields[idx]);
		vmcs12_write_field(vmcs12, vmcs_shadowing_fields[idx], val64);
	}
}

/**
 * @brief Sync shadow fields from vmcs12 to vmcs02
 *
 * @pre vcpu != NULL
 * @pre vmcs02 is current
 */
static void sync_vmcs12_to_vmcs02(struct acrn_vcpu *vcpu)
{
	uint64_t vmcs12 = (uint64_t)&vcpu->arch.nested.vmcs12;
	uint64_t val64;
	uint32_t idx;

	for (idx = 0; idx < MAX_SHADOW_VMCS_FIELDS; idx++) {
		val64 = vmcs12_read_field(vmcs12, vmcs_shadowing_fields[idx]);
		exec_vmwrite(vmcs_shadowing_fields[idx], val64);
	}

	/* Sync VMCS fields that are not shadowing */
	val64 = vmcs12_read_field(vmcs12, VMX_MSR_BITMAP_FULL);
	exec_vmwrite(VMX_MSR_BITMAP_FULL, gpa2hpa(vcpu->vm, val64));
}

/*
 * @pre vcpu != NULL
 * @pre vmcs02 is current
 */
static void flush_current_vmcs12(struct acrn_vcpu *vcpu)
{
	/*
	 * Since we have one cache VMCS12 and one active VMCS02 per vCPU,
	 * at the time of VMCLEAR current VMCS12, or VMPTRLD a new VMCS12
	 * on this vCPU, we need to sync the shadow fields from VMCS02 to
	 * cache VMCS12, and save the cache VMCS12 to guest memory.
	 */
	sync_vmcs02_to_vmcs12(vcpu);

	/* flush cached VMCS12 back to L1 guest */
	(void)copy_to_gpa(vcpu->vm, (void *)&vcpu->arch.nested.vmcs12,
		vcpu->arch.nested.current_vmcs12_ptr, sizeof(struct acrn_vmcs12));
}

/*
 * @pre vcpu != NULL
 */
static void set_vmcs02_shadow_indicator(struct acrn_vcpu *vcpu)
{
	/* vmcs02 is shadowing */
	*((uint32_t*)vcpu->arch.nested.vmcs02) |= VMCS_SHADOW_BIT_INDICATOR;
}

/*
 * @pre vcpu != NULL
 * @pre vmcs01 is current
 */
static void enable_vmcs_shadowing(struct acrn_vcpu *vcpu)
{
	uint32_t val32;

	/*
	 * This method of using the same bitmap for VMRead and VMWrite is not typical.
	 * Here we assume L1 hypervisor will not erroneously write to Read-Only fields.
	 * TODO: may use different bitmap to exclude read-only fields from VMWRITE bitmap.
	 */
	exec_vmwrite(VMX_VMREAD_BITMAP_FULL, hva2hpa(vmcs_shadowing_bitmap));
	exec_vmwrite(VMX_VMWRITE_BITMAP_FULL, hva2hpa(vmcs_shadowing_bitmap));

	/* Set VMCS shadowing bit in Secondary Proc Exec Controls */
	val32 = exec_vmread(VMX_PROC_VM_EXEC_CONTROLS2);
	val32 |= VMX_PROCBASED_CTLS2_VMCS_SHADW;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, val32);

	/* Set VMCS Link pointer */
	exec_vmwrite(VMX_VMS_LINK_PTR_FULL, hva2hpa(vcpu->arch.nested.vmcs02));
}

/*
 * @pre vcpu != NULL
 * @pre vmcs01 is current
 */
static void disable_vmcs_shadowing(void)
{
	uint32_t val32;

	/* clear VMCS shadowing bit in Secondary Proc Exec Controls */
	val32 = exec_vmread(VMX_PROC_VM_EXEC_CONTROLS2);
	val32 &= ~VMX_PROCBASED_CTLS2_VMCS_SHADW;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS2, val32);

	exec_vmwrite(VMX_VMS_LINK_PTR_FULL, ~0UL);
}

/*
 * @pre vcpu != NULL
 */
int32_t vmptrld_vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct acrn_nested *nested = &vcpu->arch.nested;
	uint64_t vmcs12_gpa;

	if (check_vmx_permission(vcpu)) {
		vmcs12_gpa = get_vmptr_gpa(vcpu);

		if (!validate_vmptr_gpa(vmcs12_gpa)) {
			nested_vmx_result(VMfailValid, VMXERR_VMPTRLD_INVALID_ADDRESS);
		} else if (vmcs12_gpa == nested->vmxon_ptr) {
			nested_vmx_result(VMfailValid, VMXERR_VMPTRLD_VMXON_POINTER);
		} else if (!validate_vmcs_revision_id(vcpu, vmcs12_gpa)) {
			nested_vmx_result(VMfailValid, VMXERR_VMPTRLD_INCORRECT_VMCS_REVISION_ID);
		} else if (nested->current_vmcs12_ptr == vmcs12_gpa) {
			/* VMPTRLD current VMCS12, do nothing */
			nested_vmx_result(VMsucceed, 0);
		} else {
			if (nested->current_vmcs12_ptr != INVALID_GPA) {
				/*
				 * L1 hypervisor VMPTRLD a new VMCS12, or VMPTRLD a VMCLEARed VMCS12.
				 * The current VMCS12 remains active but ACRN needs to sync the content of it
				 * to guest memory so that the new VMCS12 can be loaded to the cache VMCS12.
				 */

				/*
				 * Now VMCS02 is active and being used as a shadow VMCS.
				 * Disable VMCS shadowing to avoid VMCS02 will be loaded by VMPTRLD
				 * and referenced by VMCS01 as a shadow VMCS simultaneously.
				 */
				disable_vmcs_shadowing();

				/* Flush shadow VMCS to memory */
				clear_va_vmcs(nested->vmcs02);

				/* VMPTRLD the shadow VMCS so that we are able to sync it to VMCS12 */
				load_va_vmcs(nested->vmcs02);

				/* Sync shadow VMCS to cache VMCS12, and copy cache VMCS12 to L1 guest */
				flush_current_vmcs12(vcpu);

				/*
				 * The current VMCS12 has been flushed out, so that the active VMCS02
				 * needs to be VMCLEARed as well
				 */
				clear_va_vmcs(nested->vmcs02);
			}

			/* Create the VMCS02 based on this new VMCS12 */

			/*
			 * initialize VMCS02
			 * VMCS revision ID must equal to what reported by IA32_VMX_BASIC MSR
			 */
			(void)memcpy_s(nested->vmcs02, 4U, (void *)&vmx_basic, 4U);

			/*
			 * Now VMCS02 is not active, set the shadow-VMCS indicator.
			 * At L1 VM entry, VMCS02 will be referenced as a shadow VMCS.
			 */
			set_vmcs02_shadow_indicator(vcpu);

			/* VMPTRLD VMCS02 so that we can VMWRITE to it */
			load_va_vmcs(nested->vmcs02);
			init_host_state();

			/* Load VMCS12 from L1 guest memory */
			(void)copy_from_gpa(vcpu->vm, (void *)&nested->vmcs12, vmcs12_gpa,
				sizeof(struct acrn_vmcs12));

			/* Need to load shadow fields from this new VMCS12 to VMCS02 */
			sync_vmcs12_to_vmcs02(vcpu);

			/* Before VMCS02 is being used as a shadow VMCS, VMCLEAR it */
			clear_va_vmcs(nested->vmcs02);

			/* Switch back to vmcs01 */
			load_va_vmcs(vcpu->arch.vmcs);

			/* VMCS02 is referenced by VMCS01 Link Pointer */
			enable_vmcs_shadowing(vcpu);

			nested->current_vmcs12_ptr = vmcs12_gpa;
			nested_vmx_result(VMsucceed, 0);
		}
	}

	return 0;
}

void init_nested_vmx(__unused struct acrn_vm *vm)
{
	static bool initialized = false;

	if (!initialized) {
		initialized = true;

		/* Cache the value of physical MSR_IA32_VMX_BASIC */
		vmx_basic = (uint32_t)msr_read(MSR_IA32_VMX_BASIC);
	}
}
