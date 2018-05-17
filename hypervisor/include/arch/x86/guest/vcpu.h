/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VCPU_H_
#define	_VCPU_H_

#define	ACRN_VCPU_MMIO_COMPLETE		(0)

/* Size of various elements within the VCPU structure */
#define REG_SIZE                            8

/* Number of GPRs saved / restored for guest in VCPU structure */
#define NUM_GPRS                            15
#define GUEST_STATE_AREA_SIZE               512

/* Indexes of GPRs saved / restored for guest */
#define VMX_MACHINE_T_GUEST_RAX_INDEX       0
#define VMX_MACHINE_T_GUEST_RBX_INDEX       1
#define VMX_MACHINE_T_GUEST_RCX_INDEX       2
#define VMX_MACHINE_T_GUEST_RDX_INDEX       3
#define VMX_MACHINE_T_GUEST_RBP_INDEX       4
#define VMX_MACHINE_T_GUEST_RSI_INDEX       5
#define VMX_MACHINE_T_GUEST_R8_INDEX        6
#define VMX_MACHINE_T_GUEST_R9_INDEX        7
#define VMX_MACHINE_T_GUEST_R10_INDEX       8
#define VMX_MACHINE_T_GUEST_R11_INDEX       9
#define VMX_MACHINE_T_GUEST_R12_INDEX       10
#define VMX_MACHINE_T_GUEST_R13_INDEX       11
#define VMX_MACHINE_T_GUEST_R14_INDEX       12
#define VMX_MACHINE_T_GUEST_R15_INDEX       13
#define VMX_MACHINE_T_GUEST_RDI_INDEX       14

/* Offsets of GPRs for guest within the VCPU data structure */
#define VMX_MACHINE_T_GUEST_RAX_OFFSET (VMX_MACHINE_T_GUEST_RAX_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_RBX_OFFSET (VMX_MACHINE_T_GUEST_RBX_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_RCX_OFFSET (VMX_MACHINE_T_GUEST_RCX_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_RDX_OFFSET (VMX_MACHINE_T_GUEST_RDX_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_RBP_OFFSET (VMX_MACHINE_T_GUEST_RBP_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_RSI_OFFSET (VMX_MACHINE_T_GUEST_RSI_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_RDI_OFFSET (VMX_MACHINE_T_GUEST_RDI_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R8_OFFSET  (VMX_MACHINE_T_GUEST_R8_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R9_OFFSET  (VMX_MACHINE_T_GUEST_R9_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R10_OFFSET (VMX_MACHINE_T_GUEST_R10_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R11_OFFSET (VMX_MACHINE_T_GUEST_R11_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R12_OFFSET (VMX_MACHINE_T_GUEST_R12_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R13_OFFSET (VMX_MACHINE_T_GUEST_R13_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R14_OFFSET (VMX_MACHINE_T_GUEST_R14_INDEX*REG_SIZE)
#define VMX_MACHINE_T_GUEST_R15_OFFSET (VMX_MACHINE_T_GUEST_R15_INDEX*REG_SIZE)

/* Hard-coded offset of cr2 in struct run_context!! */
#define VMX_MACHINE_T_GUEST_CR2_OFFSET (128)

/* Hard-coded offset of cr2 in struct run_context!! */
#define VMX_MACHINE_T_GUEST_SPEC_CTRL_OFFSET (192)

/*sizes of various registers within the VCPU data structure */
#define VMX_CPU_S_FXSAVE_GUEST_AREA_SIZE    GUEST_STATE_AREA_SIZE

#ifndef ASSEMBLER

enum vcpu_state {
	VCPU_INIT,
	VCPU_RUNNING,
	VCPU_PAUSED,
	VCPU_ZOMBIE,
	VCPU_UNKNOWN_STATE,
};

struct cpu_regs {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rdi;
};

struct segment {
	uint64_t selector;
	uint64_t base;
	uint64_t limit;
	uint64_t attr;
};

struct run_context {
/* Contains the guest register set.
 * NOTE: This must be the first element in the structure, so that the offsets
 * in vmx_asm.S match
 */
	union {
		struct cpu_regs regs;
		uint64_t longs[NUM_GPRS];
	} guest_cpu_regs;

	/** The guests CR registers 0, 2, 3 and 4. */
	uint64_t cr0;

	/* VMX_MACHINE_T_GUEST_CR2_OFFSET =
	*  offsetof(struct run_context, cr2) = 128
	*/
	uint64_t cr2;
	uint64_t cr3;
	uint64_t cr4;

	uint64_t rip;
	uint64_t rsp;
	uint64_t rflags;

	uint64_t dr7;
	uint64_t tsc_offset;

	/* MSRs */
	/* VMX_MACHINE_T_GUEST_SPEC_CTRL_OFFSET =
	*  offsetof(struct run_context, ia32_spec_ctrl) = 192
	*/
	uint64_t ia32_spec_ctrl;
	uint64_t ia32_star;
	uint64_t ia32_lstar;
	uint64_t ia32_fmask;
	uint64_t ia32_kernel_gs_base;

	uint64_t ia32_pat;
	uint64_t ia32_efer;
	uint64_t ia32_sysenter_cs;
	uint64_t ia32_sysenter_esp;
	uint64_t ia32_sysenter_eip;
	uint64_t ia32_debugctl;

	/* segment registers */
	struct segment cs;
	struct segment ss;
	struct segment ds;
	struct segment es;
	struct segment fs;
	struct segment gs;
	struct segment tr;
	struct segment idtr;
	struct segment ldtr;
	struct segment gdtr;

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

struct vcpu_arch {
	int cur_context;
	struct run_context contexts[NR_WORLD];

	/* A pointer to the VMCS for this CPU. */
	void *vmcs;

	/* Holds the information needed for IRQ/exception handling. */
	struct {
		/* The number of the exception to raise. */
		int exception;

		/* The error number for the exception. */
		int error;
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
	uint8_t cpu_mode;
	uint8_t nr_sipi;
	uint32_t sipi_vector;

	/* interrupt injection information */
	uint64_t pending_intr;
	bool inject_event_pending;
	struct event_injection_info inject_info;

	/* per vcpu lapic */
	void *vlapic;
};

struct vm;
struct vcpu {
	int pcpu_id;	/* Physical CPU ID of this VCPU */
	int vcpu_id;	/* virtual identifier for VCPU */
	struct vcpu_arch arch_vcpu;
		/* Architecture specific definitions for this VCPU */
	struct vm *vm;		/* Reference to the VM this VCPU belongs to */
	void *entry_addr;  /* Entry address for this VCPU when first started */

	/* State of this VCPU before suspend */
	volatile enum vcpu_state prev_state;
	volatile enum vcpu_state state;	/* State of this VCPU */
	/* State of debug request for this VCPU */
	volatile enum vcpu_state dbg_req_state;
	unsigned long sync;	/*hold the bit events*/
	struct vlapic *vlapic;	/* per vCPU virtualized LAPIC */

	struct list_head run_list; /* inserted to schedule runqueue */
	unsigned long pending_pre_work; /* any pre work pending? */
	bool launched; /* Whether the vcpu is launched on target pcpu */
	unsigned int paused_cnt; /* how many times vcpu is paused */
	int running; /* vcpu is picked up and run? */
	int ioreq_pending; /* ioreq is ongoing or not? */

	struct vhm_request req; /* used by io/ept emulation */
	struct mem_io mmio; /* used by io/ept emulation */

	/* save guest msr tsc aux register.
	 * Before VMENTRY, save guest MSR_TSC_AUX to this fields.
	 * After VMEXIT, restore this fields to guest MSR_TSC_AUX.
	 * This is only temperary workaround. Once MSR emulation
	 * is enabled, we should remove this fields and related
	 * code.
	 */
	uint64_t msr_tsc_aux_guest;
	uint64_t *guest_msrs;
};

#define	is_vcpu_bsp(vcpu)	((vcpu)->vcpu_id == 0)
/* do not update Guest RIP for next VM Enter */
#define VCPU_RETAIN_RIP(vcpu)               ((vcpu)->arch_vcpu.inst_len = 0)

/* External Interfaces */
int create_vcpu(int cpu_id, struct vm *vm, struct vcpu **rtn_vcpu_handle);
int start_vcpu(struct vcpu *vcpu);
int shutdown_vcpu(struct vcpu *vcpu);
int destroy_vcpu(struct vcpu *vcpu);

void reset_vcpu(struct vcpu *vcpu);
void init_vcpu(struct vcpu *vcpu);
void pause_vcpu(struct vcpu *vcpu, enum vcpu_state new_state);
void resume_vcpu(struct vcpu *vcpu);
void schedule_vcpu(struct vcpu *vcpu);
int prepare_vcpu(struct vm *vm, int pcpu_id);

void request_vcpu_pre_work(struct vcpu *vcpu, int pre_work_id);

#endif

#endif
