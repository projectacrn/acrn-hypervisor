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

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

static int unhandled_vmexit_handler(struct vcpu *vcpu);
static int xsetbv_vmexit_handler(struct vcpu *vcpu);
/* VM Dispatch table for Exit condition handling */
static const struct vm_exit_dispatch dispatch_table[] = {
	[VMX_EXIT_REASON_EXCEPTION_OR_NMI] = {
		.handler = exception_vmexit_handler},
	[VMX_EXIT_REASON_EXTERNAL_INTERRUPT] = {
		.handler = external_interrupt_vmexit_handler},
	[VMX_EXIT_REASON_TRIPLE_FAULT] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INIT_SIGNAL] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_STARTUP_IPI] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_IO_SMI] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_OTHER_SMI] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INTERRUPT_WINDOW] = {
		.handler = interrupt_window_vmexit_handler},
	[VMX_EXIT_REASON_NMI_WINDOW] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_TASK_SWITCH] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_CPUID] = {
		.handler = cpuid_vmexit_handler},
	[VMX_EXIT_REASON_GETSEC] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_HLT] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INVD] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INVLPG] = {
		.handler = unhandled_vmexit_handler,},
	[VMX_EXIT_REASON_RDPMC] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_RDTSC] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_RSM] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMCALL] = {
		.handler = vmcall_vmexit_handler},
	[VMX_EXIT_REASON_VMCLEAR] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMLAUNCH] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMPTRLD] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMPTRST] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMREAD] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMRESUME] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMWRITE] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMXOFF] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMXON] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_CR_ACCESS] = {
		.handler = cr_access_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_DR_ACCESS] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_IO_INSTRUCTION] = {
		.handler = io_instr_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_RDMSR] = {
		.handler = rdmsr_vmexit_handler},
	[VMX_EXIT_REASON_WRMSR] = {
		.handler = wrmsr_vmexit_handler},
	[VMX_EXIT_REASON_ENTRY_FAILURE_INVALID_GUEST_STATE] = {
		.handler = unhandled_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_ENTRY_FAILURE_MSR_LOADING] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_MWAIT] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_MONITOR_TRAP] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_MONITOR] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_PAUSE] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_ENTRY_FAILURE_MACHINE_CHECK] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_TPR_BELOW_THRESHOLD] = {
		.handler = tpr_below_threshold_vmexit_handler},
	[VMX_EXIT_REASON_APIC_ACCESS] = {
		.handler = apic_access_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_VIRTUALIZED_EOI] = {
		.handler = veoi_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_GDTR_IDTR_ACCESS] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_LDTR_TR_ACCESS] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_EPT_VIOLATION] = {
		.handler = ept_violation_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_EPT_MISCONFIGURATION] = {
		.handler = ept_misconfig_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_INVEPT] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_RDTSCP] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INVVPID] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_WBINVD] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_XSETBV] = {
		.handler = xsetbv_vmexit_handler},
	[VMX_EXIT_REASON_APIC_WRITE] = {
		.handler = apic_write_vmexit_handler,
		.need_exit_qualification = 1}
};

int vmexit_handler(struct vcpu *vcpu)
{
	struct vm_exit_dispatch *dispatch = HV_NULL;
	uint16_t basic_exit_reason;
	int ret;

	if ((int)get_cpu_id() != vcpu->pcpu_id) {
		pr_fatal("vcpu is not running on its pcpu!");
		return -EINVAL;
	}

	/* Obtain interrupt info */
	vcpu->arch_vcpu.idt_vectoring_info =
	    exec_vmread(VMX_IDT_VEC_INFO_FIELD);

	/* Calculate basic exit reason (low 16-bits) */
	basic_exit_reason = vcpu->arch_vcpu.exit_reason & 0xFFFF;

	/* Log details for exit */
	pr_dbg("Exit Reason: 0x%016llx ", vcpu->arch_vcpu.exit_reason);

	/* Ensure exit reason is within dispatch table */
	if (basic_exit_reason < ARRAY_SIZE(dispatch_table)) {
		/* Calculate dispatch table entry */
		dispatch = (struct vm_exit_dispatch *)
			(dispatch_table + basic_exit_reason);

		/* See if an exit qualification is necessary for this exit
		 * handler
		 */
		if (dispatch->need_exit_qualification) {
			/* Get exit qualification */
			vcpu->arch_vcpu.exit_qualification =
			    exec_vmread(VMX_EXIT_QUALIFICATION);
		}
	}

	/* Update current vcpu in VM that caused vm exit */
	vcpu->vm->current_vcpu = vcpu;

	/* exit dispatch handling */
	if (basic_exit_reason == VMX_EXIT_REASON_EXTERNAL_INTERRUPT) {
		/* Handling external_interrupt
		 * should disable intr
		 */
		ret = dispatch->handler(vcpu);
	} else {
		CPU_IRQ_ENABLE();
		ret = dispatch->handler(vcpu);
		CPU_IRQ_DISABLE();
	}

	return ret;
}

static int unhandled_vmexit_handler(__unused struct vcpu *vcpu)
{
	pr_fatal("Error: Unhandled VM exit condition from guest at 0x%016llx ",
			exec_vmread(VMX_GUEST_RIP));

	pr_fatal("Exit Reason: 0x%016llx ", vcpu->arch_vcpu.exit_reason);

	pr_err("Exit qualification: 0x%016llx ",
			exec_vmread(VMX_EXIT_QUALIFICATION));

	/* while(1); */

	TRACE_2L(TRC_VMEXIT_UNHANDLED, vcpu->arch_vcpu.exit_reason, 0);

	return 0;
}

static int write_cr0(struct vcpu *vcpu, uint64_t value)
{
	uint32_t value32;
	uint64_t value64;

	pr_dbg("VMM: Guest trying to write 0x%08x to CR0", value);

	/* Read host mask value */
	value64 = exec_vmread(VMX_CR0_MASK);

	/* Clear all bits being written by guest that are owned by host */
	value &= ~value64;

	/* Update CR0 in guest state */
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr0 |= value;
	exec_vmwrite(VMX_GUEST_CR0,
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr0);
	pr_dbg("VMM: Guest allowed to write 0x%08x to CR0",
		  vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr0);

	/* If guest is trying to transition vcpu from unpaged real mode to page
	 * protected mode make necessary changes to VMCS structure to reflect
	 * transition from real mode to paged-protected mode
	 */
	if (!is_vcpu_bsp(vcpu) &&
	    (vcpu->arch_vcpu.cpu_mode == CPU_MODE_REAL) &&
	    (value & CR0_PG) && (value & CR0_PE)) {
		/* Enable protected mode */
		value32 = exec_vmread(VMX_ENTRY_CONTROLS);
		value32 |= (VMX_ENTRY_CTLS_IA32E_MODE |
			    VMX_ENTRY_CTLS_LOAD_PAT |
			    VMX_ENTRY_CTLS_LOAD_EFER);
		exec_vmwrite(VMX_ENTRY_CONTROLS, value32);
		pr_dbg("VMX_ENTRY_CONTROLS: 0x%x ", value32);

		/* Set up EFER */
		value64 = exec_vmread64(VMX_GUEST_IA32_EFER_FULL);
		value64 |= (MSR_IA32_EFER_SCE_BIT |
			    MSR_IA32_EFER_LME_BIT |
			    MSR_IA32_EFER_LMA_BIT | MSR_IA32_EFER_NXE_BIT);
		exec_vmwrite64(VMX_GUEST_IA32_EFER_FULL, value64);
		pr_dbg("VMX_GUEST_IA32_EFER: 0x%016llx ", value64);
	}

	return 0;
}

static int write_cr3(struct vcpu *vcpu, uint64_t value)
{
	/* Write to guest's CR3 */
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr3 = value;

	/* Commit new value to VMCS */
	exec_vmwrite(VMX_GUEST_CR3,
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr3);

	return 0;
}

static int write_cr4(struct vcpu *vcpu, uint64_t value)
{
	uint64_t temp64;

	pr_dbg("VMM: Guest trying to write 0x%08x to CR4", value);

	/* Read host mask value */
	temp64 = exec_vmread(VMX_CR4_MASK);

	/* Clear all bits being written by guest that are owned by host */
	value &= ~temp64;

	/* Write updated CR4 (bitwise OR of allowed guest bits and CR4 host
	 * value)
	 */
	vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr4 |= value;
	exec_vmwrite(VMX_GUEST_CR4,
		vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr4);
	pr_dbg("VMM: Guest allowed to write 0x%08x to CR4",
		  vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr4);

	return 0;
}

static int read_cr3(struct vcpu *vcpu, uint64_t *value)
{
	*value = vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context].cr3;

	pr_dbg("VMM: reading 0x%08x from CR3", *value);

	return 0;
}

int cpuid_vmexit_handler(struct vcpu *vcpu)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	guest_cpuid(vcpu,
		(uint32_t *)&cur_context->guest_cpu_regs.regs.rax,
		(uint32_t *)&cur_context->guest_cpu_regs.regs.rbx,
		(uint32_t *)&cur_context->guest_cpu_regs.regs.rcx,
		(uint32_t *)&cur_context->guest_cpu_regs.regs.rdx);

	TRACE_2L(TRC_VMEXIT_CPUID, vcpu->vcpu_id, 0);

	return 0;
}

int cr_access_vmexit_handler(struct vcpu *vcpu)
{
	uint64_t *regptr;
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	static const int reg_trans_tab[] = {
		[0] = VMX_MACHINE_T_GUEST_RAX_INDEX,
		[1] = VMX_MACHINE_T_GUEST_RCX_INDEX,
		[2] = VMX_MACHINE_T_GUEST_RDX_INDEX,
		[3] = VMX_MACHINE_T_GUEST_RBX_INDEX,
		[4] = 0xFF, /* for sp reg, should not be used, just for init */
		[5] = VMX_MACHINE_T_GUEST_RBP_INDEX,
		[6] = VMX_MACHINE_T_GUEST_RSI_INDEX,
		[7] = VMX_MACHINE_T_GUEST_RDI_INDEX,
		[8] = VMX_MACHINE_T_GUEST_R8_INDEX,
		[9] = VMX_MACHINE_T_GUEST_R9_INDEX,
		[10] = VMX_MACHINE_T_GUEST_R10_INDEX,
		[11] = VMX_MACHINE_T_GUEST_R11_INDEX,
		[12] = VMX_MACHINE_T_GUEST_R12_INDEX,
		[13] = VMX_MACHINE_T_GUEST_R13_INDEX,
		[14] = VMX_MACHINE_T_GUEST_R14_INDEX,
		[15] = VMX_MACHINE_T_GUEST_R15_INDEX
	};
	int idx = VM_EXIT_CR_ACCESS_REG_IDX(vcpu->arch_vcpu.exit_qualification);

	ASSERT(idx != 4, "index should not be 4 (target SP)");
	regptr = cur_context->guest_cpu_regs.longs + reg_trans_tab[idx];

	switch ((VM_EXIT_CR_ACCESS_ACCESS_TYPE
		 (vcpu->arch_vcpu.exit_qualification) << 4) |
		VM_EXIT_CR_ACCESS_CR_NUM(vcpu->arch_vcpu.exit_qualification)) {
	case 0x00:
		/* mov to cr0 */
		write_cr0(vcpu, *regptr);
		break;

	case 0x03:
		/* mov to cr3 */
		write_cr3(vcpu, *regptr);
		break;

	case 0x04:
		/* mov to cr4 */
		write_cr4(vcpu, *regptr);
		break;

	case 0x13:
		/* mov from cr3 */
		read_cr3(vcpu, regptr);
		break;
#if 0
	case 0x14:
		/* mov from cr4 (this should not happen) */
	case 0x10:
		/* mov from cr0 (this should not happen) */
#endif
	case 0x08:
		/* mov to cr8 */
		vlapic_set_cr8(vcpu->arch_vcpu.vlapic, *regptr);
		break;
	case 0x18:
		/* mov from cr8 */
		*regptr = vlapic_get_cr8(vcpu->arch_vcpu.vlapic);
		break;
	default:
		panic("Unhandled CR access");
		return -EINVAL;
	}

	TRACE_2L(TRC_VMEXIT_CR_ACCESS,
		VM_EXIT_CR_ACCESS_ACCESS_TYPE
			(vcpu->arch_vcpu.exit_qualification),
		VM_EXIT_CR_ACCESS_CR_NUM
			(vcpu->arch_vcpu.exit_qualification));

	return 0;
}

#if 0
/*
 * VMX_PROCBASED_CTLS_INVLPG is not enabled in the VM-execution
 * control therefore we don't need it's handler.
 *
 * INVLPG: this instruction Invalidates any translation lookaside buffer
 */
int invlpg_handler(__unused struct vcpu *vcpu)
{
	pr_fatal("INVLPG executed");

	return 0;
}
#endif

/*
 * XSETBV instruction set's the XCR0 that is used to tell for which
 * components states can be saved on a context switch using xsave.
 */
static int xsetbv_vmexit_handler(struct vcpu *vcpu)
{
	int idx;
	uint64_t val64;
	struct run_context *ctx_ptr;

	val64 = exec_vmread(VMX_GUEST_CR4);
	if (!(val64 & CR4_OSXSAVE)) {
		vcpu_inject_gp(vcpu);
		return -1;
	}

	idx = vcpu->arch_vcpu.cur_context;
	if (idx >= NR_WORLD)
		return -1;

	ctx_ptr = &(vcpu->arch_vcpu.contexts[idx]);

	/*to access XCR0,'rcx' should be 0*/
	if (ctx_ptr->guest_cpu_regs.regs.rcx != 0) {
		vcpu_inject_gp(vcpu);
		return -1;
	}

	val64 = ((ctx_ptr->guest_cpu_regs.regs.rax) & 0xffffffff) |
			(ctx_ptr->guest_cpu_regs.regs.rdx << 32);

	/*bit 0(x87 state) of XCR0 can't be cleared*/
	if (!(val64 & 0x01)) {
		vcpu_inject_gp(vcpu);
		return -1;
	}

	/*XCR0[2:1] (SSE state & AVX state) can't not be
	 *set to 10b as it is necessary to set both bits
	 *to use AVX instructions.
	 **/
	if (((val64 >> 1) & 0x3) == 0x2) {
		vcpu_inject_gp(vcpu);
		return -1;
	}

	write_xcr(0, val64);
	return 0;
}
