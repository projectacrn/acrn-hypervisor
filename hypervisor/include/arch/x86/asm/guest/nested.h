/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef NESTED_H
#define NESTED_H

#include <asm/vm_config.h>
#include <lib/errno.h>

/* helper data structure to make VMX capability MSR manipulation easier */
union value_64 {
	uint64_t full;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	} u;
};

/*
 * Following MSRs are supported if nested virtualization is enabled
 * - If CONFIG_NVMX_ENABLED is set, these MSRs are included in emulated_guest_msrs[]
 * - otherwise, they are included in unsupported_msrs[]
 */
#define NUM_VMX_MSRS	20U
#define LIST_OF_VMX_MSRS	\
	MSR_IA32_SMBASE,	\
	MSR_IA32_VMX_BASIC,			\
	MSR_IA32_VMX_PINBASED_CTLS,		\
	MSR_IA32_VMX_PROCBASED_CTLS,		\
	MSR_IA32_VMX_EXIT_CTLS,			\
	MSR_IA32_VMX_ENTRY_CTLS,		\
	MSR_IA32_VMX_MISC,			\
	MSR_IA32_VMX_CR0_FIXED0,		\
	MSR_IA32_VMX_CR0_FIXED1,		\
	MSR_IA32_VMX_CR4_FIXED0,		\
	MSR_IA32_VMX_CR4_FIXED1,		\
	MSR_IA32_VMX_VMCS_ENUM,			\
	MSR_IA32_VMX_PROCBASED_CTLS2,		\
	MSR_IA32_VMX_EPT_VPID_CAP,		\
	MSR_IA32_VMX_TRUE_PINBASED_CTLS,	\
	MSR_IA32_VMX_TRUE_PROCBASED_CTLS,	\
	MSR_IA32_VMX_TRUE_EXIT_CTLS,		\
	MSR_IA32_VMX_TRUE_ENTRY_CTLS,		\
	MSR_IA32_VMX_VMFUNC,			\
	MSR_IA32_VMX_PROCBASED_CTLS3

/* refer to ISDM APPENDIX B: FIELD ENCODING IN VMCS */
#define VMX_VMCS_FIELD_ACCESS_HIGH(v)		(((v) >> 0U) & 0x1U)
#define VMX_VMCS_FIELD_INDEX(v)			(((v) >> 1U) & 0x1ffU)
#define VMX_VMCS_FIELD_TYPE(v)			(((v) >> 10U) & 0x3U)
#define VMX_VMCS_FIELD_TYPE_CTL			(0U)
#define VMX_VMCS_FIELD_TYPE_VMEXIT		(1U)
#define VMX_VMCS_FIELD_TYPE_GUEST		(2U)
#define VMX_VMCS_FIELD_TYPE_HOST		(3U)
#define VMX_VMCS_FIELD_WIDTH(v)			(((v) >> 13U) & 0x3U)
#define VMX_VMCS_FIELD_WIDTH_16			(0U)
#define VMX_VMCS_FIELD_WIDTH_64			(1U)
#define VMX_VMCS_FIELD_WIDTH_32			(2U)
#define VMX_VMCS_FIELD_WIDTH_NATURAL		(3U)

/*
 * VM-Exit Instruction-Information Field
 *
 * ISDM Vol 3C Table 27-9: INVEPT, INVPCID, INVVPID
 * ISDM Vol 3C Table 27-13: VMCLEAR, VMPTRLD, VMPTRST, VMXON, XRSTORS, and XSAVES.
 * ISDM Vol 3C Table 27-14: VMREAD and VMWRITE
 *
 * Either Table 27-9 or Table 27-13 is a subset of Table 27-14, so we are able to
 * define the following macros to be used for the above mentioned instructions.
 */
#define VMX_II_SCALING(v)			(((v) >> 0U) & 0x3U)
#define VMX_II_REG1(v)				(((v) >> 3U) & 0xfU)
#define VMX_II_ADDR_SIZE(v)			(((v) >> 7U) & 0x7U)
#define VMX_II_IS_REG(v)			(((v) >> 10U) & 0x1U)
#define VMX_II_SEG_REG(v)			(((v) >> 15U) & 0x7U)
#define VMX_II_IDX_REG(v)			(((v) >> 18U) & 0xfU)
#define VMX_II_IDX_REG_VALID(v)			((((v) >> 22U) & 0x1U) == 0U)
#define VMX_II_BASE_REG(v)			(((v) >> 23U) & 0xfU)
#define VMX_II_BASE_REG_VALID(v)		((((v) >> 27U) & 0x1U) == 0U)
#define VMX_II_REG2(v)				(((v) >> 28U) & 0xfU)

#define VMCS_SHADOW_BIT_INDICATOR		(1U << 31U)

/* refer to ISDM: Table 30-1. VM-Instruction Error Numbers */
#define VMXERR_VMCLEAR_VMXON_POINTER		(3)
#define VMXERR_VMLAUNCH_NONCLEAR_VMCS		(4)
#define VMXERR_VMRESUME_NONLAUNCHED_VMCS	(5)
#define VMXERR_VMRESUME_AFTER_VMXOFF		(6)
#define VMXERR_VMPTRLD_INVALID_ADDRESS		(9)
#define VMXERR_VMPTRLD_INCORRECT_VMCS_REVISION_ID (10)
#define VMXERR_VMPTRLD_VMXON_POINTER		(11)
#define VMXERR_UNSUPPORTED_COMPONENT		(12)
#define VMXERR_VMWRITE_RO_COMPONENT		(13)
#define VMXERR_VMXON_IN_VMX_ROOT_OPERATION	(15)
#define VMXERR_INVEPT_INVVPID_INVALID_OPERAND	(28)

/*
 * This VMCS12 revision id is chosen arbitrarily.
 * The emulated MSR_IA32_VMX_BASIC returns this ID in bits 30:0.
 */
#define VMCS12_REVISION_ID		0x15407E12U

#define VMCS12_LAUNCH_STATE_CLEAR		(0U)
#define VMCS12_LAUNCH_STATE_LAUNCHED		(1U)

/*
 * struct acrn_vmcs12 describes the emulated VMCS for the nested guest (L2).
 */
struct acrn_vmcs12 {
	uint8_t vmcs_hdr[4];
	uint32_t abort;

	/*
	 * Rest of the memory is used for "VMCS Data"
	 * Layout of VMCS Data is non-architectural and processor
	 * implemetation specific.
	 */
	uint32_t launch_state;

	/* 16-bit Control Fields */
	uint16_t vpid;
	uint16_t posted_intr_nv;
	uint16_t eptp_index;

	/* 16-bit Read-only Fields */
	uint16_t padding;

	/* 16-bit Guest-State Fields */
	uint16_t guest_es;
	uint16_t guest_cs;
	uint16_t guest_ss;
	uint16_t guest_ds;
	uint16_t guest_fs;
	uint16_t guest_gs;
	uint16_t guest_ldtr;
	uint16_t guest_tr;
	uint16_t guest_intr_status;
	uint16_t pml_index;

	/* 16-bit Host-State Fields */
	uint16_t host_es;
	uint16_t host_cs;
	uint16_t host_ss;
	uint16_t host_ds;
	uint16_t host_fs;
	uint16_t host_gs;
	uint16_t host_tr;

	/* 64-bit Control Fields */
	uint64_t io_bitmap_a;
	uint64_t io_bitmap_b;
	uint64_t msr_bitmap;
	uint64_t vm_exit_msr_store_addr;
	uint64_t vm_exit_msr_load_addr;
	uint64_t vm_entry_load_addr;
	uint64_t executive_vmcs_ptr;
	uint64_t pml_addr;
	uint64_t tsc_offset;
	uint64_t virtual_apic_addr;
	uint64_t apic_access_addr;
	uint64_t posted_interrupt_desc_addr;
	uint64_t vm_func_controls;
	uint64_t ept_pointer;
	uint64_t eoi_exit_bitmap0;
	uint64_t eoi_exit_bitmap1;
	uint64_t eoi_exit_bitmap2;
	uint64_t eoi_exit_bitmap3;
	uint64_t eptp_list_addr;
	uint64_t vmread_bitmap_addr;
	uint64_t vmwrite_bitmap_addr;
	uint64_t virt_exception_info_addr;
	uint64_t xss_exiting_bitmap;
	uint64_t encls_exiting_bitmap;
	uint64_t sub_page_permission_ptr;
	uint64_t tsc_multiplier;

	/* 64-bit Read-Only Data Fields */
	uint64_t guest_phys_addr;

	/* 64-bit Guest-State Fields */
	uint64_t vmcs_link_ptr;
	uint64_t guest_ia32_debugctl;
	uint64_t guest_ia32_pat;
	uint64_t guest_ia32_efer;
	uint64_t ia32_perf_global_ctrl;
	uint64_t guest_pdpte0;
	uint64_t guest_pdpte1;
	uint64_t guest_pdpte2;
	uint64_t guest_pdpte3;
	uint64_t guest_ia32_bndcfgs;
	uint64_t guest_ia32_rtit_ctl;

	/* 64-bit Host-State Fields */
	uint64_t host_ia32_pat;
	uint64_t host_ia32_efer;
	uint64_t host_ia32_perf_global_ctrl;

	/* 32-bit Control Fields */
	uint32_t pin_based_exec_ctrl;
	uint32_t proc_based_exec_ctrl;
	uint32_t exception_bitmap;
	uint32_t page_fault_error_code_mask;
	uint32_t page_fault_error_code_match;
	uint32_t cr3_target_count;
	uint32_t vm_exit_controls;
	uint32_t vm_exit_msr_store_count;
	uint32_t vm_exit_msr_load_count;
	uint32_t vm_entry_controls;
	uint32_t vm_entry_msr_load_count;
	uint32_t vm_entry_intr_info_field;
	uint32_t vm_entry_exception_err_code;
	uint32_t vm_entry_instr_len;
	uint32_t tpr_threshold;
	uint32_t proc_based_exec_ctrl2;
	uint32_t ple_gap;
	uint32_t ple_window;

	/* 32-bit Read-Only Data Fields */
	uint32_t vm_instr_error;
	uint32_t exit_reason;
	uint32_t vm_exit_intr_info;
	uint32_t vm_exit_intr_error_code;
	uint32_t idt_vectoring_info_field;
	uint32_t idt_vectoring_error_code;
	uint32_t vm_exit_instr_len;
	uint32_t vm_exit_instr_info;

	/* 32-bit Guest-State Fields */
	uint32_t guest_es_limit;
	uint32_t guest_cs_limit;
	uint32_t guest_ss_limit;
	uint32_t guest_ds_limit;
	uint32_t guest_fs_limit;
	uint32_t guest_gs_limit;
	uint32_t guest_ldtr_limit;
	uint32_t guest_tr_limit;
	uint32_t guest_gdtr_limit;
	uint32_t guest_idtr_limit;
	uint32_t guest_es_ar;
	uint32_t guest_cs_ar;
	uint32_t guest_ss_ar;
	uint32_t guest_ds_ar;
	uint32_t guest_fs_ar;
	uint32_t guest_gs_ar;
	uint32_t guest_ldtr_ar;
	uint32_t guest_tr_ar;
	uint32_t guest_intr_state;
	uint32_t guest_activity_state;
	uint32_t guest_smbase;
	uint32_t guest_ia32_sysenter_cs;
	uint32_t vmx_preempt_timer_val;

	/* 32-bit Host-State Fields */
	uint32_t host_ia32_sysenter_cs;

	/* Natural-width Control Fields */
	uint64_t cr0_guest_host_mask;
	uint64_t cr4_guest_host_mask;
	uint64_t cr0_read_shadow;
	uint64_t cr4_read_shadow;
	uint64_t cr3_target_val0;
	uint64_t cr3_target_val1;
	uint64_t cr3_target_val2;
	uint64_t cr3_target_val3;

	/* Natural-width Read-Only Data Fields */
	uint64_t exit_qual;
	uint64_t io_rcx;
	uint64_t io_rsi;
	uint64_t io_rdi;
	uint64_t io_rip;
	uint64_t guest_linear_addr;

	/* Natural-width Guest-State Fields */
	uint64_t guest_cr0;
	uint64_t guest_cr3;
	uint64_t guest_cr4;
	uint64_t guest_es_base;
	uint64_t guest_cs_base;
	uint64_t guest_ss_base;
	uint64_t guest_ds_base;
	uint64_t guest_fs_base;
	uint64_t guest_gs_base;
	uint64_t guest_ldtr_base;
	uint64_t guest_tr_base;
	uint64_t guest_gdtr_base;
	uint64_t guest_idtr_base;
	uint64_t guest_dr7;
	uint64_t guest_rsp;
	uint64_t guest_rip;
	uint64_t guest_rflags;
	uint64_t guest_pending_debug_excp;
	uint64_t guest_ia32_sysenter_esp;
	uint64_t guest_ia32_sysenter_eip;

	/** Natural-width Host-State Fields */
	uint64_t host_cr0;
	uint64_t host_cr3;
	uint64_t host_cr4;
	uint64_t host_fs_base;
	uint64_t host_gs_base;
	uint64_t host_tr_base;
	uint64_t host_gdtr_base;
	uint64_t host_idtr_base;
	uint64_t host_ia32_sysenter_esp;
	uint64_t host_ia32_sysenter_eip;
	uint64_t host_rsp;
	uint64_t host_rip;
};

enum VMXResult {
	VMsucceed,
	VMfailValid,
	VMfailInvalid,
};
void nested_vmx_result(enum VMXResult, int error_number);
int64_t get_invvpid_ept_operands(struct acrn_vcpu *vcpu, void *desc, size_t size);
bool check_vmx_permission(struct acrn_vcpu *vcpu);
int32_t vmxon_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmxoff_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmptrld_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmclear_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmread_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmwrite_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmresume_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t vmlaunch_vmexit_handler(struct acrn_vcpu *vcpu);
int32_t invvpid_vmexit_handler(struct acrn_vcpu *vcpu);

#ifdef CONFIG_NVMX_ENABLED
struct acrn_vvmcs {
	uint8_t vmcs02[PAGE_SIZE];	/* VMCS to run L2 and as Link Pointer in VMCS01 */
	struct acrn_vmcs12 vmcs12;	/* To cache L1's VMCS12*/
	uint64_t vmcs12_gpa;            /* The corresponding L1 GPA for this VMCS12 */
	uint32_t ref_cnt;		/* Count of being VMPTRLDed without VMCLEARed */
	bool host_state_dirty;		/* To indicate need to merge VMCS12 host-state fields to VMCS01 */
	bool control_fields_dirty;	/* For all other non-host-state fields that need to be merged */
} __aligned(PAGE_SIZE);

#define MAX_ACTIVE_VVMCS_NUM	4

struct acrn_nested {
	struct acrn_vvmcs vvmcs[MAX_ACTIVE_VVMCS_NUM];
	struct acrn_vvmcs *current_vvmcs;	/* Refer to the current loaded VMCS12 */
	uint64_t vmxon_ptr;		/* GPA */
	bool vmxon;		/* To indicate if vCPU entered VMX operation */
	bool in_l2_guest;	/* To indicate if vCPU is currently in Guest mode (from L1's perspective) */
} __aligned(PAGE_SIZE);

void init_nested_vmx(__unused struct acrn_vm *vm);
bool is_vcpu_in_l2_guest(struct acrn_vcpu *vcpu);
bool is_vmx_msr(uint32_t msr);
void init_vmx_msrs(struct acrn_vcpu *vcpu);
int32_t read_vmx_msr(__unused struct acrn_vcpu *vcpu, uint32_t msr, uint64_t *val);
int32_t nested_vmexit_handler(struct acrn_vcpu *vcpu);
#else
struct acrn_nested {};

static inline void init_nested_vmx(__unused struct acrn_vm *vm) {}
static inline bool is_vcpu_in_l2_guest(__unused struct acrn_vcpu *vcpu) {
	return false;
}

static inline bool is_vmx_msr(__unused uint32_t msr)
{
	/*
	 * if nested virtualization is disabled, return false so that
	 * it can be treated as unsupported MSR.
	 */
	return false;
}

static inline void init_vmx_msrs(__unused struct acrn_vcpu *vcpu) {}

static inline int32_t read_vmx_msr(__unused struct acrn_vcpu *vcpu,
	__unused uint32_t msr, __unused uint64_t *val)
{
	return -EACCES;
}

static inline int32_t nested_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	return -EINVAL;
}
#endif /* CONFIG_NVMX_ENABLED */
#endif /* NESTED_H */
