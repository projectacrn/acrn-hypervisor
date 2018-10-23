/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCPU_H
#define VCPU_H

#define	ACRN_VCPU_MMIO_COMPLETE		(0U)

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

#include <guest.h>

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

struct run_context {
/* Contains the guest register set.
 * NOTE: This must be the first element in the structure, so that the offsets
 * in vmx_asm.S match
 */
	union {
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
	uint64_t vmx_ia32_pat;
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

struct event_injection_info {
	uint32_t intr_info;
	uint32_t error_code;
};

struct cpu_context {
	struct run_context run_ctx;
	struct ext_context ext_ctx;
};

struct vcpu_arch {
	/* vmcs region for this vcpu, MUST be 4KB-aligned */
	uint8_t vmcs[CPU_PAGE_SIZE];
	/* per vcpu lapic */
	struct acrn_vlapic vlapic;
	int cur_context;
	struct cpu_context contexts[NR_WORLD];

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

	/* Auxiliary TSC value */
	uint64_t msr_tsc_aux;

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

} __aligned(CPU_PAGE_SIZE);

struct vm;
struct vcpu {
	/* Architecture specific definitions for this VCPU */
	struct vcpu_arch arch_vcpu;
	uint16_t pcpu_id;	/* Physical CPU ID of this VCPU */
	uint16_t vcpu_id;	/* virtual identifier for VCPU */
	struct vm *vm;		/* Reference to the VM this VCPU belongs to */

	/* State of this VCPU before suspend */
	volatile enum vcpu_state prev_state;
	volatile enum vcpu_state state;	/* State of this VCPU */
	/* State of debug request for this VCPU */
	volatile enum vcpu_state dbg_req_state;
	uint64_t sync;	/*hold the bit events*/

	struct list_head run_list; /* inserted to schedule runqueue */
	uint64_t pending_pre_work; /* any pre work pending? */
	bool launched; /* Whether the vcpu is launched on target pcpu */
	uint32_t paused_cnt; /* how many times vcpu is paused */
	uint32_t running; /* vcpu is picked up and run? */

	struct io_request req; /* used by io/ept emulation */

	/* save guest msr tsc aux register.
	 * Before VMENTRY, save guest MSR_TSC_AUX to this fields.
	 * After VMEXIT, restore this fields to guest MSR_TSC_AUX.
	 * This is only temperary workaround. Once MSR emulation
	 * is enabled, we should remove this fields and related
	 * code.
	 */
	uint64_t msr_tsc_aux_guest;
	uint64_t guest_msrs[IDX_MAX_MSR];
#ifdef CONFIG_MTRR_ENABLED
	struct mtrr_state mtrr;
#endif /* CONFIG_MTRR_ENABLED */
	uint64_t reg_cached;
	uint64_t reg_updated;
} __aligned(CPU_PAGE_SIZE);

struct vcpu_dump {
	struct vcpu *vcpu;
	char *str;
	int str_max;
};

static inline bool is_vcpu_bsp(const struct vcpu *vcpu)
{
	return (vcpu->vcpu_id == BOOT_CPU_ID);
}

/* do not update Guest RIP for next VM Enter */
static inline void vcpu_retain_rip(struct vcpu *vcpu)
{
	(vcpu)->arch_vcpu.inst_len = 0U;
}

static inline struct acrn_vlapic *
vcpu_vlapic(struct vcpu *vcpu)
{
	return &(vcpu->arch_vcpu.vlapic);
}

/* External Interfaces */
uint64_t vcpu_get_gpreg(const struct vcpu *vcpu, uint32_t reg);
void vcpu_set_gpreg(struct vcpu *vcpu, uint32_t reg, uint64_t val);
uint64_t vcpu_get_rip(struct vcpu *vcpu);
void vcpu_set_rip(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_rsp(struct vcpu *vcpu);
void vcpu_set_rsp(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_efer(struct vcpu *vcpu);
void vcpu_set_efer(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_rflags(struct vcpu *vcpu);
void vcpu_set_rflags(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_cr0(struct vcpu *vcpu);
void vcpu_set_cr0(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_cr2(struct vcpu *vcpu);
void vcpu_set_cr2(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_cr4(struct vcpu *vcpu);
void vcpu_set_cr4(struct vcpu *vcpu, uint64_t val);
uint64_t vcpu_get_pat_ext(const struct vcpu *vcpu);
void vcpu_set_pat_ext(struct vcpu *vcpu, uint64_t val);
void set_vcpu_regs(struct vcpu *vcpu, struct acrn_vcpu_regs *vcpu_regs);
void reset_vcpu_regs(struct vcpu *vcpu);
void set_ap_entry(struct vcpu *vcpu, uint64_t entry);

static inline bool is_long_mode(struct vcpu *vcpu)
{
	return (vcpu_get_efer(vcpu) & MSR_IA32_EFER_LMA_BIT) != 0UL;
}

static inline bool is_paging_enabled(struct vcpu *vcpu)
{
	return (vcpu_get_cr0(vcpu) & CR0_PG) != 0UL;
}

static inline bool is_pae(struct vcpu *vcpu)
{
	return (vcpu_get_cr4(vcpu) & CR4_PAE) != 0UL;
}

struct vcpu* get_ever_run_vcpu(uint16_t pcpu_id);
int create_vcpu(uint16_t pcpu_id, struct vm *vm, struct vcpu **rtn_vcpu_handle);
/*
 *  @pre vcpu != NULL
 */
int run_vcpu(struct vcpu *vcpu);
int shutdown_vcpu(struct vcpu *vcpu);
/*
 *  @pre vcpu != NULL
 */
void offline_vcpu(struct vcpu *vcpu);

void reset_vcpu(struct vcpu *vcpu);
void pause_vcpu(struct vcpu *vcpu, enum vcpu_state new_state);
void resume_vcpu(struct vcpu *vcpu);
void schedule_vcpu(struct vcpu *vcpu);
int prepare_vcpu(struct vm *vm, uint16_t pcpu_id);

void request_vcpu_pre_work(struct vcpu *vcpu, uint16_t pre_work_id);

void vcpu_dumpreg(void *data);
#endif /* ASSEMBLER */

#endif /* VCPU_H */
