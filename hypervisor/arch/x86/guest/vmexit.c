/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/vmx.h>
#include <asm/guest/virq.h>
#include <asm/mmu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/vm_reset.h>
#include <asm/guest/vmx_io.h>
#include <asm/guest/lock_instr_emul.h>
#include <asm/guest/ept.h>
#include <asm/guest/vept.h>
#include <asm/vtd.h>
#include <asm/cpuid.h>
#include <asm/guest/vcpuid.h>
#include <trace.h>
#include <asm/rtcm.h>
#include <debug/console.h>
#include <per_cpu.h>

/*
 * According to "SDM APPENDIX C VMX BASIC EXIT REASONS",
 * there are 65 Basic Exit Reasons.
 */
#define NR_VMX_EXIT_REASONS	70U

static int32_t triple_fault_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t unhandled_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t xsetbv_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t wbinvd_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t undefined_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t pause_vmexit_handler(__unused struct acrn_vcpu *vcpu);
static int32_t hlt_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t mtf_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t loadiwkey_vmexit_handler(struct acrn_vcpu *vcpu);
static int32_t init_signal_vmexit_handler(__unused struct acrn_vcpu *vcpu);
static int32_t mwait_monitor_vmexit_handler (struct acrn_vcpu *vcpu);

/* VM Dispatch table for Exit condition handling */
static const struct vm_exit_dispatch dispatch_table[NR_VMX_EXIT_REASONS] = {
	[VMX_EXIT_REASON_EXCEPTION_OR_NMI] = {
		.handler = exception_vmexit_handler},
	[VMX_EXIT_REASON_EXTERNAL_INTERRUPT] = {
		.handler = external_interrupt_vmexit_handler},
	[VMX_EXIT_REASON_TRIPLE_FAULT] = {
		.handler = triple_fault_vmexit_handler},
	[VMX_EXIT_REASON_INIT_SIGNAL] = {
		.handler = init_signal_vmexit_handler},
	[VMX_EXIT_REASON_STARTUP_IPI] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_IO_SMI] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_OTHER_SMI] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INTERRUPT_WINDOW] = {
		.handler = interrupt_window_vmexit_handler},
	[VMX_EXIT_REASON_NMI_WINDOW] = {
		.handler = nmi_window_vmexit_handler},
	[VMX_EXIT_REASON_TASK_SWITCH] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_CPUID] = {
		.handler = cpuid_vmexit_handler},
	[VMX_EXIT_REASON_GETSEC] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_HLT] = {
		.handler = hlt_vmexit_handler},
	[VMX_EXIT_REASON_INVD] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INVLPG] = {
		.handler = unhandled_vmexit_handler,},
	[VMX_EXIT_REASON_RDPMC] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_RDTSC] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_RSM] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMCALL] = {
		.handler = vmcall_vmexit_handler},
	[VMX_EXIT_REASON_VMPTRST] = {
		.handler = undefined_vmexit_handler},
#ifndef CONFIG_NVMX_ENABLED
	[VMX_EXIT_REASON_VMLAUNCH] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMRESUME] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMCLEAR] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMPTRLD] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMREAD] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMWRITE] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMXOFF] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_VMXON] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_INVEPT] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_INVVPID] = {
		.handler = undefined_vmexit_handler},
#else
	[VMX_EXIT_REASON_VMLAUNCH] = {
		.handler = vmlaunch_vmexit_handler},
	[VMX_EXIT_REASON_VMRESUME] = {
		.handler = vmresume_vmexit_handler},
	[VMX_EXIT_REASON_VMCLEAR] = {
		.handler = vmclear_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_VMPTRLD] = {
		.handler = vmptrld_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_VMREAD] = {
		.handler = vmread_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_VMWRITE] = {
		.handler = vmwrite_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_VMXOFF] = {
		.handler = vmxoff_vmexit_handler},
	[VMX_EXIT_REASON_VMXON] = {
		.handler = vmxon_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_INVEPT] = {
		.handler = invept_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_INVVPID] = {
		.handler = invvpid_vmexit_handler,
		.need_exit_qualification = 1},
#endif
	[VMX_EXIT_REASON_CR_ACCESS] = {
		.handler = cr_access_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_DR_ACCESS] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_IO_INSTRUCTION] = {
		.handler = pio_instr_vmexit_handler,
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
		.handler = mwait_monitor_vmexit_handler},
	[VMX_EXIT_REASON_MONITOR_TRAP] = {
		.handler = mtf_vmexit_handler},
	[VMX_EXIT_REASON_MONITOR] = {
		.handler = mwait_monitor_vmexit_handler},
	[VMX_EXIT_REASON_PAUSE] = {
		.handler = pause_vmexit_handler},
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
	[VMX_EXIT_REASON_RDTSCP] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_WBINVD] = {
		.handler = wbinvd_vmexit_handler},
	[VMX_EXIT_REASON_XSETBV] = {
		.handler = xsetbv_vmexit_handler},
	[VMX_EXIT_REASON_APIC_WRITE] = {
		.handler = apic_write_vmexit_handler,
		.need_exit_qualification = 1},
	[VMX_EXIT_REASON_RDRAND] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_INVPCID] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_VMFUNC] = {
		.handler = undefined_vmexit_handler},
	[VMX_EXIT_REASON_ENCLS] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_RDSEED] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_PAGE_MODIFICATION_LOG_FULL] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_XSAVES] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_XRSTORS] = {
		.handler = unhandled_vmexit_handler},
	[VMX_EXIT_REASON_LOADIWKEY] = {
		.handler = loadiwkey_vmexit_handler}
};

int32_t vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct vm_exit_dispatch *dispatch = NULL;
	uint16_t basic_exit_reason;
	int32_t ret;

	if (get_pcpu_id() != pcpuid_from_vcpu(vcpu)) {
		pr_fatal("vcpu is not running on its pcpu!");
		ret = -EINVAL;
	} else if (is_vcpu_in_l2_guest(vcpu)) {
		ret = nested_vmexit_handler(vcpu);
	} else {
		/* Obtain interrupt info */
		vcpu->arch.idt_vectoring_info = exec_vmread32(VMX_IDT_VEC_INFO_FIELD);
		/* Filter out HW exception & NMI */
		if ((vcpu->arch.idt_vectoring_info & VMX_INT_INFO_VALID) != 0U) {
			uint32_t vector_info = vcpu->arch.idt_vectoring_info;
			uint32_t vector = vector_info & 0xffU;
			uint32_t type = (vector_info & VMX_INT_TYPE_MASK) >> 8U;
			uint32_t err_code = 0U;

			if (type == VMX_INT_TYPE_HW_EXP) {
				if ((vector_info & VMX_INT_INFO_ERR_CODE_VALID) != 0U) {
					err_code = exec_vmread32(VMX_IDT_VEC_ERROR_CODE);
				}
				(void)vcpu_queue_exception(vcpu, vector, err_code);
				vcpu->arch.idt_vectoring_info = 0U;
			} else if (type == VMX_INT_TYPE_NMI) {
				vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
				vcpu->arch.idt_vectoring_info = 0U;
			} else {
				/* No action on EXT_INT or SW exception. */
			}
		}

		/* Calculate basic exit reason (low 16-bits) */
		basic_exit_reason = (uint16_t)(vcpu->arch.exit_reason & 0xFFFFU);

		/* Log details for exit */
		pr_dbg("Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);

		/* Ensure exit reason is within dispatch table */
		if (basic_exit_reason >= ARRAY_SIZE(dispatch_table)) {
			pr_err("Invalid Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);
			ret = -EINVAL;
		} else {
			/* Calculate dispatch table entry */
			dispatch = (struct vm_exit_dispatch *)(dispatch_table + basic_exit_reason);

			/* See if an exit qualification is necessary for this exit handler */
			if (dispatch->need_exit_qualification != 0U) {
				/* Get exit qualification */
				vcpu->arch.exit_qualification = exec_vmread(VMX_EXIT_QUALIFICATION);
			}

			/* exit dispatch handling */
			if (basic_exit_reason == VMX_EXIT_REASON_EXTERNAL_INTERRUPT) {
				/* Handling external_interrupt should disable intr */
				if (!is_lapic_pt_enabled(vcpu)) {
					CPU_IRQ_DISABLE_ON_CONFIG();
				}

				ret = dispatch->handler(vcpu);

				if (!is_lapic_pt_enabled(vcpu)) {
					CPU_IRQ_ENABLE_ON_CONFIG();
				}
			} else {
				ret = dispatch->handler(vcpu);
			}
		}
	}

	console_vmexit_callback(vcpu);

	return ret;
}

static int32_t mwait_monitor_vmexit_handler (struct acrn_vcpu *vcpu)
{
	pr_fatal("Error: Unsupported mwait option from guest at 0x%016lx ",
		exec_vmread(VMX_GUEST_RIP));

	vcpu_inject_ud(vcpu);

	return 0;
}

static int32_t unhandled_vmexit_handler(struct acrn_vcpu *vcpu)
{
	pr_fatal("Error: Unhandled VM exit condition from guest at 0x%016lx ",
			exec_vmread(VMX_GUEST_RIP));

	pr_fatal("Exit Reason: 0x%016lx ", vcpu->arch.exit_reason);

	pr_err("Exit qualification: 0x%016lx ",
			exec_vmread(VMX_EXIT_QUALIFICATION));

	TRACE_2L(TRACE_VMEXIT_UNHANDLED, vcpu->arch.exit_reason, 0UL);

	return 0;
}

/* MTF is currently only used for split-lock emulation */
static int32_t mtf_vmexit_handler(struct acrn_vcpu *vcpu)
{
	vcpu->arch.proc_vm_exec_ctrls &= ~(VMX_PROCBASED_CTLS_MON_TRAP);
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, vcpu->arch.proc_vm_exec_ctrls);

	vcpu_retain_rip(vcpu);

	if (vcpu->arch.emulating_lock) {
		vcpu->arch.emulating_lock = false;
		vcpu_complete_lock_instr_emulation(vcpu);
	}

	return 0;
}

static int32_t triple_fault_vmexit_handler(struct acrn_vcpu *vcpu)
{
	pr_fatal("VM%d: triple fault @ guest RIP 0x%016lx, exit qualification: 0x%016lx",
		vcpu->vm->vm_id, exec_vmread(VMX_GUEST_RIP), exec_vmread(VMX_EXIT_QUALIFICATION));
	triple_fault_shutdown_vm(vcpu);

	return 0;
}

static int32_t pause_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	yield_current();
	return 0;
}

static int32_t hlt_vmexit_handler(struct acrn_vcpu *vcpu)
{
	if ((vcpu->arch.pending_req == 0UL) && (!vlapic_has_pending_intr(vcpu))) {
		wait_event(&vcpu->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
	}
	return 0;
}

int32_t cpuid_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t eax, ebx, ecx, edx;

	eax = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RAX);
	ebx = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RBX);
	ecx = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RCX);
	edx = (uint32_t)vcpu_get_gpreg(vcpu, CPU_REG_RDX);
	TRACE_2L(TRACE_VMEXIT_CPUID, (uint64_t)eax, (uint64_t)ecx);
	guest_cpuid(vcpu, &eax, &ebx, &ecx, &edx);
	vcpu_set_gpreg(vcpu, CPU_REG_RAX, (uint64_t)eax);
	vcpu_set_gpreg(vcpu, CPU_REG_RBX, (uint64_t)ebx);
	vcpu_set_gpreg(vcpu, CPU_REG_RCX, (uint64_t)ecx);
	vcpu_set_gpreg(vcpu, CPU_REG_RDX, (uint64_t)edx);

	return 0;
}

/*
 * XSETBV instruction set's the XCR0 that is used to tell for which
 * components states can be saved on a context switch using xsave.
 *
 * According to SDM vol3 25.1.1:
 * Invalid-opcode exception (UD) and faults based on privilege level (include
 * virtual-8086 mode previleged instructions are not recognized) have higher
 * priority than VM exit.
 *
 * According to SDM vol2 - XSETBV instruction description:
 * If CR4.OSXSAVE[bit 18] = 0,
 * execute "XSETBV" instruction will generate #UD exception.
 * So VM exit won't happen with VMX_GUEST_CR4.CR4_OSXSAVE = 0.
 * CR4_OSXSAVE bit is controlled by guest (CR4_OSXSAVE bit
 * is set as guest expect to see).
 *
 * We don't need to handle those case here because we depends on VMX to handle
 * them.
 */
static int32_t xsetbv_vmexit_handler(struct acrn_vcpu *vcpu)
{
	int32_t idx, ret = -1;	/* ret < 0 call vcpu_inject_gp(vcpu, 0U) */
	uint32_t cpl;
	uint64_t val64;

	if (vcpu->arch.xsave_enabled && ((vcpu_get_cr4(vcpu) & CR4_OSXSAVE) != 0UL)) {
		idx = vcpu->arch.cur_context;
		/* get current privilege level */
		cpl = exec_vmread32(VMX_GUEST_CS_ATTR);
		cpl = (cpl >> 5U) & 3U;

		if ((idx < NR_WORLD) && (cpl == 0U)) {
			/* to access XCR0,'ecx' should be 0 */
			if ((vcpu_get_gpreg(vcpu, CPU_REG_RCX) & 0xffffffffUL) == 0UL) {
				val64 = (vcpu_get_gpreg(vcpu, CPU_REG_RAX) & 0xffffffffUL) |
						(vcpu_get_gpreg(vcpu, CPU_REG_RDX) << 32U);

				/* bit 0(x87 state) of XCR0 can't be cleared */
				if (((val64 & 0x01UL) != 0UL) && ((val64 & XCR0_RESERVED_BITS) == 0UL)) {
					/*
					 * XCR0[2:1] (SSE state & AVX state) can't not be
					 * set to 10b as it is necessary to set both bits
					 * to use AVX instructions.
					 */
					if ((val64 & (XCR0_SSE | XCR0_AVX)) != XCR0_AVX) {
						/*
						 * SDM Vol.1 13-4, XCR0[4:3] are associated with MPX state,
						 * Guest should not set these two bits without MPX support.
						 */
						if ((val64 & (XCR0_BNDREGS | XCR0_BNDCSR)) == 0UL) {
							write_xcr(0, val64);
							ret = 0;
						}
					}
				}
			}
		}
	} else {
		/* CPUID.01H:ECX.XSAVE[bit 26] = 0 */
		vcpu_inject_ud(vcpu);
		ret = 0;
	}

	return ret;
}

static int32_t wbinvd_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint16_t i;
	struct acrn_vcpu *other;

	/* GUEST_FLAG_RT has not set in post-launched RTVM before it has been created */
	if ((!is_software_sram_enabled()) && (!has_rt_vm())) {
		flush_invalidate_all_cache();
	} else {
		if (is_rt_vm(vcpu->vm)) {
			walk_ept_table(vcpu->vm, ept_flush_leaf_page);
		} else {
			spinlock_obtain(&vcpu->vm->wbinvd_lock);
			/* Pause other vcpus and let them wait for the wbinvd completion */
			foreach_vcpu(i, vcpu->vm, other) {
				if (other != vcpu) {
					vcpu_make_request(other, ACRN_REQUEST_WAIT_WBINVD);
				}
			}

			walk_ept_table(vcpu->vm, ept_flush_leaf_page);

			foreach_vcpu(i, vcpu->vm, other) {
				if (other != vcpu) {
					signal_event(&other->events[VCPU_EVENT_SYNC_WBINVD]);
				}
			}
			spinlock_release(&vcpu->vm->wbinvd_lock);
		}
	}

	return 0;
}

static int32_t loadiwkey_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint64_t xmm[6] = {0};

	/* Wrapping key nobackup and randomization are not supported */
	if ((vcpu_get_gpreg(vcpu, CPU_REG_RAX) != 0UL)) {
		vcpu_inject_gp(vcpu, 0);
	} else {
		read_xmm_0_2(&xmm[0], &xmm[2], &xmm[4]);
		vcpu->arch.IWKey.encryption_key[0] = xmm[2];
		vcpu->arch.IWKey.encryption_key[1] = xmm[3];
		vcpu->arch.IWKey.encryption_key[2] = xmm[4];
		vcpu->arch.IWKey.encryption_key[3] = xmm[5];
		vcpu->arch.IWKey.integrity_key[0] = xmm[0];
		vcpu->arch.IWKey.integrity_key[1] = xmm[1];

		asm_loadiwkey(0);
		get_cpu_var(arch.whose_iwkey) = vcpu;
	}

	return 0;
}

/*
 * This handler is only triggered by INIT signal when poweroff from inside of RTVM
 */
static int32_t init_signal_vmexit_handler(__unused struct acrn_vcpu *vcpu)
{
	/*
	 * Intel SDM Volume 3, 25.2:
	 *   INIT signals. INIT signals cause VM exits. A logical processer performs none
	 *   of the operations normally associated with these events. Such exits do not modify
	 *   register state or clear pending events as they would outside of VMX operation (If
	 *   a logical processor is the wait-for-SIPI state, INIT signals are blocked. They do
	 *   not cause VM exits in this case).
	 *
	 * So, it is safe to ignore the signal but need retain its RIP.
	 */
	vcpu_retain_rip(vcpu);
	return 0;
}

/*
 * vmexit handler for just injecting a #UD exception
 * ACRN doesn't enable VMFUNC, VMFUNC treated as undefined.
 */
static int32_t undefined_vmexit_handler(struct acrn_vcpu *vcpu)
{
	vcpu_inject_ud(vcpu);
	return 0;
}
