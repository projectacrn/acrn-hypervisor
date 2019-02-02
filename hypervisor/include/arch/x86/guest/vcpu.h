/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file vcpu.h
 *
 * @brief public APIs for vcpu operations
 */

#ifndef VCPU_H
#define VCPU_H

/* Number of GPRs saved / restored for guest in VCPU structure */
#define NUM_GPRS                            16U
#define GUEST_STATE_AREA_SIZE               512

#define	CPU_CONTEXT_OFFSET_RAX			0U
#define	CPU_CONTEXT_OFFSET_RCX			8U
#define	CPU_CONTEXT_OFFSET_RDX			16U
#define	CPU_CONTEXT_OFFSET_RBX			24U
#define	CPU_CONTEXT_OFFSET_RSP			32U
#define	CPU_CONTEXT_OFFSET_RBP			40U
#define	CPU_CONTEXT_OFFSET_RSI			48U
#define	CPU_CONTEXT_OFFSET_RDI			56U
#define	CPU_CONTEXT_OFFSET_R8			64U
#define	CPU_CONTEXT_OFFSET_R9			72U
#define	CPU_CONTEXT_OFFSET_R10			80U
#define	CPU_CONTEXT_OFFSET_R11			88U
#define	CPU_CONTEXT_OFFSET_R12			96U
#define	CPU_CONTEXT_OFFSET_R13			104U
#define	CPU_CONTEXT_OFFSET_R14			112U
#define	CPU_CONTEXT_OFFSET_R15			120U
#define	CPU_CONTEXT_OFFSET_CR0			128U
#define	CPU_CONTEXT_OFFSET_CR2			136U
#define	CPU_CONTEXT_OFFSET_CR4			144U
#define	CPU_CONTEXT_OFFSET_RIP			152U
#define	CPU_CONTEXT_OFFSET_RFLAGS		160U
#define	CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL	168U
#define	CPU_CONTEXT_OFFSET_IA32_EFER		176U
#define	CPU_CONTEXT_OFFSET_EXTCTX_START		184U
#define	CPU_CONTEXT_OFFSET_CR3			184U
#define	CPU_CONTEXT_OFFSET_IDTR			192U
#define	CPU_CONTEXT_OFFSET_LDTR			216U

/*sizes of various registers within the VCPU data structure */
#define VMX_CPU_S_FXSAVE_GUEST_AREA_SIZE    GUEST_STATE_AREA_SIZE

#ifndef ASSEMBLER

#include <guest_memory.h>
#include <virtual_cr.h>
#include <vlapic.h>
#include <vmtrr.h>
#include <schedule.h>
#include <io_req.h>
#include <msr.h>

/**
 * @brief vcpu
 *
 * @defgroup acrn_vcpu ACRN vcpu
 * @{
 */

/*
 * VCPU related APIs
 */
#define ACRN_REQUEST_EXCP		0U
#define ACRN_REQUEST_EVENT		1U
#define ACRN_REQUEST_EXTINT		2U
#define ACRN_REQUEST_NMI		3U
#define ACRN_REQUEST_EOI_EXIT_UPDATE	4U
#define ACRN_REQUEST_EPT_FLUSH		5U
#define ACRN_REQUEST_TRP_FAULT		6U
#define ACRN_REQUEST_VPID_FLUSH		7U /* flush vpid tlb */

#define save_segment(seg, SEG_NAME)				\
{								\
	(seg).selector = exec_vmread16(SEG_NAME##_SEL);		\
	(seg).base = exec_vmread(SEG_NAME##_BASE);		\
	(seg).limit = exec_vmread32(SEG_NAME##_LIMIT);		\
	(seg).attr = exec_vmread32(SEG_NAME##_ATTR);		\
}

#define load_segment(seg, SEG_NAME)				\
{								\
	exec_vmwrite16(SEG_NAME##_SEL, (seg).selector);		\
	exec_vmwrite(SEG_NAME##_BASE, (seg).base);		\
	exec_vmwrite32(SEG_NAME##_LIMIT, (seg).limit);		\
	exec_vmwrite32(SEG_NAME##_ATTR, (seg).attr);		\
}

/* Define segments constants for guest */
#define REAL_MODE_BSP_INIT_CODE_SEL     (0xf000U)
#define REAL_MODE_DATA_SEG_AR           (0x0093U)
#define REAL_MODE_CODE_SEG_AR           (0x009fU)
#define PROTECTED_MODE_DATA_SEG_AR      (0xc093U)
#define PROTECTED_MODE_CODE_SEG_AR      (0xc09bU)
#define REAL_MODE_SEG_LIMIT             (0xffffU)
#define PROTECTED_MODE_SEG_LIMIT        (0xffffffffU)
#define DR7_INIT_VALUE                  (0x400UL)
#define LDTR_AR                         (0x0082U) /* LDT, type must be 2, refer to SDM Vol3 26.3.1.2 */
#define TR_AR                           (0x008bU) /* TSS (busy), refer to SDM Vol3 26.3.1.2 */

#define foreach_vcpu(idx, vm, vcpu)				\
	for ((idx) = 0U, (vcpu) = &((vm)->hw.vcpu_array[(idx)]);	\
		(idx) < (vm)->hw.created_vcpus;			\
		(idx)++, (vcpu) = &((vm)->hw.vcpu_array[(idx)])) \
		if (vcpu->state != VCPU_OFFLINE)

enum vcpu_state {
	VCPU_INIT,
	VCPU_RUNNING,
	VCPU_PAUSED,
	VCPU_ZOMBIE,
	VCPU_OFFLINE,
	VCPU_UNKNOWN_STATE,
};

enum vm_cpu_mode {
	CPU_MODE_REAL,
	CPU_MODE_PROTECTED,
	CPU_MODE_COMPATIBILITY,		/* IA-32E mode (CS.L = 0) */
	CPU_MODE_64BIT,			/* IA-32E mode (CS.L = 1) */
};

struct segment_sel {
	uint16_t selector;
	uint64_t base;
	uint32_t limit;
	uint32_t attr;
};

/**
 * @brief registers info saved for vcpu running context
 */
struct run_context {
/* Contains the guest register set.
 * NOTE: This must be the first element in the structure, so that the offsets
 * in vmx_asm.S match
 */
	union guest_cpu_regs_t {
		struct acrn_gp_regs regs;
		uint64_t longs[NUM_GPRS];
	} guest_cpu_regs;

	/** The guests CR registers 0, 2, 3 and 4. */
	uint64_t cr0;

	/* CPU_CONTEXT_OFFSET_CR2 =
	*  offsetof(struct run_context, cr2) = 136
	*/
	uint64_t cr2;
	uint64_t cr4;

	uint64_t rip;
	uint64_t rflags;

	/* CPU_CONTEXT_OFFSET_IA32_SPEC_CTRL =
	*  offsetof(struct run_context, ia32_spec_ctrl) = 168
	*/
	uint64_t ia32_spec_ctrl;
	uint64_t ia32_efer;
};

/*
 * extended context does not save/restore during vm exity/entry, it's mainly
 * used in trusty world switch
 */
struct ext_context {
	uint64_t cr3;

	/* segment registers */
	struct segment_sel idtr;
	struct segment_sel ldtr;
	struct segment_sel gdtr;
	struct segment_sel tr;
	struct segment_sel cs;
	struct segment_sel ss;
	struct segment_sel ds;
	struct segment_sel es;
	struct segment_sel fs;
	struct segment_sel gs;

	uint64_t ia32_star;
	uint64_t ia32_lstar;
	uint64_t ia32_fmask;
	uint64_t ia32_kernel_gs_base;

	uint64_t ia32_pat;
	uint32_t ia32_sysenter_cs;
	uint64_t ia32_sysenter_esp;
	uint64_t ia32_sysenter_eip;
	uint64_t ia32_debugctl;

	uint64_t dr7;
	uint64_t tsc_offset;

	uint64_t vmx_cr0;
	uint64_t vmx_cr4;
	uint64_t vmx_cr0_read_shadow;
	uint64_t vmx_cr4_read_shadow;

	/* The 512 bytes area to save the FPU/MMX/SSE states for the guest */
	uint64_t
	fxstore_guest_area[VMX_CPU_S_FXSAVE_GUEST_AREA_SIZE / sizeof(uint64_t)]
	__aligned(16);
};

/* 2 worlds: 0 for Normal World, 1 for Secure World */
#define NR_WORLD	2
#define NORMAL_WORLD	0
#define SECURE_WORLD	1

#define NUM_WORLD_MSRS		2U
#define NUM_COMMON_MSRS		7U
#define NUM_GUEST_MSRS		(NUM_WORLD_MSRS + NUM_COMMON_MSRS)

#define EOI_EXIT_BITMAP_SIZE	256U

struct event_injection_info {
	uint32_t intr_info;
	uint32_t error_code;
};

struct cpu_context {
	struct run_context run_ctx;
	struct ext_context ext_ctx;

	/* per world MSRs, need isolation between secure and normal world */
	uint32_t world_msrs[NUM_WORLD_MSRS];
};

/* Intel SDM 24.8.2, the address must be 16-byte aligned */
struct msr_store_entry {
	uint32_t msr_num;
	uint32_t reserved;
	uint64_t value;
} __aligned(16);

enum {
	MSR_AREA_TSC_AUX = 0,
	MSR_AREA_COUNT,
};

struct msr_store_area {
	struct msr_store_entry guest[MSR_AREA_COUNT];
	struct msr_store_entry host[MSR_AREA_COUNT];
};

struct acrn_vcpu_arch {
	/* vmcs region for this vcpu, MUST be 4KB-aligned */
	uint8_t vmcs[PAGE_SIZE];
	/* per vcpu lapic */
	struct acrn_vlapic vlapic;

#ifdef CONFIG_MTRR_ENABLED
	struct acrn_vmtrr vmtrr;
#endif

	int32_t cur_context;
	struct cpu_context contexts[NR_WORLD];

	/* common MSRs, world_msrs[] is a subset of it */
	uint64_t guest_msrs[NUM_GUEST_MSRS];

	uint16_t vpid;

	/* Holds the information needed for IRQ/exception handling. */
	struct {
		/* The number of the exception to raise. */
		uint32_t exception;

		/* The error number for the exception. */
		uint32_t error;
	} exception_info;

	uint8_t lapic_mask;
	uint32_t irq_window_enabled;
	uint32_t nrexits;

	/* VCPU context state information */
	uint32_t exit_reason;
	uint32_t idt_vectoring_info;
	uint64_t exit_qualification;
	uint32_t inst_len;

	/* Information related to secondary / AP VCPU start-up */
	enum vm_cpu_mode cpu_mode;
	uint8_t nr_sipi;

	/* interrupt injection information */
	uint64_t pending_req;
	bool inject_event_pending;
	struct event_injection_info inject_info;

	/* List of MSRS to be stored and loaded on VM exits or VM entries */
	struct msr_store_area msr_area;

	/* EOI_EXIT_BITMAP buffer, for the bitmap update */
	uint64_t eoi_exit_bitmap[EOI_EXIT_BITMAP_SIZE >> 6U];
	spinlock_t lock;
} __aligned(PAGE_SIZE);

struct acrn_vm;
struct acrn_vcpu {
	uint8_t stack[CONFIG_STACK_SIZE] __aligned(16);

	/* Architecture specific definitions for this VCPU */
	struct acrn_vcpu_arch arch;
	uint16_t pcpu_id;	/* Physical CPU ID of this VCPU */
	uint16_t vcpu_id;	/* virtual identifier for VCPU */
	struct acrn_vm *vm;		/* Reference to the VM this VCPU belongs to */

	/* State of this VCPU before suspend */
	volatile enum vcpu_state prev_state;
	volatile enum vcpu_state state;	/* State of this VCPU */

	struct sched_object sched_obj;
	bool launched; /* Whether the vcpu is launched on target pcpu */
	uint32_t running; /* vcpu is picked up and run? */

	struct io_request req; /* used by io/ept emulation */

	uint64_t reg_cached;
	uint64_t reg_updated;
} __aligned(PAGE_SIZE);

struct vcpu_dump {
	struct acrn_vcpu *vcpu;
	char *str;
	uint32_t str_max;
};

static inline bool is_vcpu_bsp(const struct acrn_vcpu *vcpu)
{
	return (vcpu->vcpu_id == BOOT_CPU_ID);
}

/* do not update Guest RIP for next VM Enter */
static inline void vcpu_retain_rip(struct acrn_vcpu *vcpu)
{
	(vcpu)->arch.inst_len = 0U;
}

static inline struct acrn_vlapic *
vcpu_vlapic(struct acrn_vcpu *vcpu)
{
	return &(vcpu->arch.vlapic);
}

void default_idle(__unused struct sched_object *obj);
void vcpu_thread(struct sched_object *obj);

int32_t vmx_vmrun(struct run_context *context, int32_t ops, int32_t ibrs);

/* External Interfaces */

/**
 * @brief get vcpu register value
 *
 * Get target vCPU's general purpose registers value in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] reg register of the vcpu
 *
 * @return the value of the register.
 */
uint64_t vcpu_get_gpreg(const struct acrn_vcpu *vcpu, uint32_t reg);

/**
 * @brief set vcpu register value
 *
 * Set target vCPU's general purpose registers value in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] reg register of the vcpu
 * @param[in] val the value set the register of the vcpu
 */
void vcpu_set_gpreg(struct acrn_vcpu *vcpu, uint32_t reg, uint64_t val);

/**
 * @brief get vcpu RIP value
 *
 * Get & cache target vCPU's RIP in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of RIP.
 */
uint64_t vcpu_get_rip(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu RIP value
 *
 * Update target vCPU's RIP in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set RIP
 */
void vcpu_set_rip(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu RSP value
 *
 * Get & cache target vCPU's RSP in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of RSP.
 */
uint64_t vcpu_get_rsp(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu RSP value
 *
 * Update target vCPU's RSP in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set RSP
 */
void vcpu_set_rsp(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu EFER value
 *
 * Get & cache target vCPU's EFER in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of EFER.
 */
uint64_t vcpu_get_efer(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu EFER value
 *
 * Update target vCPU's EFER in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set EFER
 */
void vcpu_set_efer(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get vcpu RFLAG value
 *
 * Get & cache target vCPU's RFLAGS in run_context.
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return the value of RFLAGS.
 */
uint64_t vcpu_get_rflags(struct acrn_vcpu *vcpu);

/**
 * @brief set vcpu RFLAGS value
 *
 * Update target vCPU's RFLAGS in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] val the value set RFLAGS
 */
void vcpu_set_rflags(struct acrn_vcpu *vcpu, uint64_t val);

/**
 * @brief get guest emulated MSR
 *
 * Get the content of emulated guest MSR
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] msr the guest MSR
 *
 * @return the value of emulated MSR.
 */
uint64_t vcpu_get_guest_msr(const struct acrn_vcpu *vcpu, uint32_t msr);

/**
 * @brief set guest emulated MSR
 *
 * Update the content of emulated guest MSR
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] msr the guest MSR
 * @param[in] val the value to set the target MSR
 *
 */
void vcpu_set_guest_msr(struct acrn_vcpu *vcpu, uint32_t msr, uint64_t val);

/**
 * @brief write eoi_exit_bitmap to VMCS fields
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return void
 */
void vcpu_set_vmcs_eoi_exit(struct acrn_vcpu *vcpu);

/**
 * @brief reset eoi_exit_bitmap
 *
 * @param[in] vcpu pointer to vcpu data structure
 *
 * @return void
 */

void vcpu_reset_eoi_exit_all(struct acrn_vcpu *vcpu);

/**
 * @brief set eoi_exit_bitmap bit
 *
 * Set corresponding bit of vector in eoi_exit_bitmap
 *
 * @param[in] vcpu pointer to vcpu data structure
 * @param[in] vector
 *
 * @return void 
 */
void vcpu_set_eoi_exit(struct acrn_vcpu *vcpu, uint32_t vector);
/**
 * @brief set all the vcpu registers
 *
 * Update target vCPU's all registers in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] vcpu_regs all the registers' value
 */
void set_vcpu_regs(struct acrn_vcpu *vcpu, struct acrn_vcpu_regs *vcpu_regs);

/**
 * @brief reset all the vcpu registers
 *
 * Reset target vCPU's all registers in run_context to initial values.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 */
void reset_vcpu_regs(struct acrn_vcpu *vcpu);

/**
 * @brief set the vcpu AP entry
 *
 * Set target vCPU's AP running entry in run_context.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] entry the entry value for AP
 */
void set_ap_entry(struct acrn_vcpu *vcpu, uint64_t entry);

static inline bool is_long_mode(struct acrn_vcpu *vcpu)
{
	return (vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) != 0UL;
}

static inline bool is_paging_enabled(struct acrn_vcpu *vcpu)
{
	return (vcpu_get_cr0(vcpu) & CR0_PG) != 0UL;
}

static inline bool is_pae(struct acrn_vcpu *vcpu)
{
	return (vcpu_get_cr4(vcpu) & CR4_PAE) != 0UL;
}

struct acrn_vcpu* get_ever_run_vcpu(uint16_t pcpu_id);

/**
 * @brief create a vcpu for the target vm
 *
 * Creates/allocates a vCPU instance, with initialization for its vcpu_id,
 * vpid, vmcs, vlapic, etc. It sets the init vCPU state to VCPU_INIT
 *
 * @param[in] pcpu_id created vcpu will run on this pcpu
 * @param[in] vm pointer to vm data structure, this vcpu will owned by this vm.
 * @param[out] rtn_vcpu_handle pointer to the created vcpu
 *
 * @retval 0 vcpu created successfully, other values failed.
 */
int32_t create_vcpu(uint16_t pcpu_id, struct acrn_vm *vm, struct acrn_vcpu **rtn_vcpu_handle);

/**
 * @brief run into non-root mode based on vcpu setting
 *
 * An interface in vCPU thread to implement VM entry and VM exit.
 * A CPU switches between VMX root mode and non-root mode based on it.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 *
 * @retval 0 vcpu run successfully, other values failed.
 */
int32_t run_vcpu(struct acrn_vcpu *vcpu);

int32_t shutdown_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief unmap the vcpu with pcpu and free its vlapic
 *
 * Unmap the vcpu with pcpu and free its vlapic, and set the vcpu state to offline
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @pre vcpu != NULL
 */
void offline_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief reset vcpu state and values
 *
 * Reset all fields in a vCPU instance, the vCPU state is reset to VCPU_INIT.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 */
void reset_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief pause the vcpu and set new state
 *
 * Change a vCPU state to VCPU_PAUSED or VCPU_ZOMBIE, and make a reschedule request for it.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 * @param[in] new_state the state to set vcpu
 */
void pause_vcpu(struct acrn_vcpu *vcpu, enum vcpu_state new_state);

/**
 * @brief resume the vcpu
 *
 * Change a vCPU state to VCPU_RUNNING, and make a reschedule request for it.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 */
void resume_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief set the vcpu to running state, then it will be scheculed.
 *
 * Adds a vCPU into the run queue and make a reschedule request for it. It sets the vCPU state to VCPU_RUNNING.
 *
 * @param[inout] vcpu pointer to vcpu data structure
 */
void schedule_vcpu(struct acrn_vcpu *vcpu);

/**
 * @brief create a vcpu for the vm and mapped to the pcpu.
 *
 * Create a vcpu for the vm, and mapped to the pcpu.
 *
 * @param[inout] vm pointer to vm data structure
 * @param[in] pcpu_id which the vcpu will be mapped
 */
int32_t prepare_vcpu(struct acrn_vm *vm, uint16_t pcpu_id);

/**
 * @brief get physical destination cpu mask
 *
 * get the corresponding physical destination cpu mask for the vm and virtual destination cpu mask
 *
 * @param[in] vm pointer to vm data structure
 * @param[in] vdmask virtual destination cpu mask
 */
uint64_t vcpumask2pcpumask(struct acrn_vm *vm, uint64_t vdmask);

/**
 * @}
 */
/* End of acrn_vcpu */

#endif /* ASSEMBLER */

#endif /* VCPU_H */
