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

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>

#define EXCEPTION_ERROR_CODE_VALID  8
#define INTERRPUT_QUEUE_BUFF_SIZE   255

#define ACRN_DBG_INTR	6

static const uint16_t exception_type[] = {
	[0] = VMX_INT_TYPE_HW_EXP,
	[1] = VMX_INT_TYPE_HW_EXP,
	[2] = VMX_INT_TYPE_HW_EXP,
	[3] = VMX_INT_TYPE_HW_EXP,
	[4] = VMX_INT_TYPE_HW_EXP,
	[5] = VMX_INT_TYPE_HW_EXP,
	[6] = VMX_INT_TYPE_HW_EXP,
	[7] = VMX_INT_TYPE_HW_EXP,
	[8] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[9] = VMX_INT_TYPE_HW_EXP,
	[10] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[11] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[12] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[13] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[14] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[15] = VMX_INT_TYPE_HW_EXP,
	[16] = VMX_INT_TYPE_HW_EXP,
	[17] = VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID,
	[18] = VMX_INT_TYPE_HW_EXP,
	[19] = VMX_INT_TYPE_HW_EXP,
	[20] = VMX_INT_TYPE_HW_EXP,
	[21] = VMX_INT_TYPE_HW_EXP,
	[22] = VMX_INT_TYPE_HW_EXP,
	[23] = VMX_INT_TYPE_HW_EXP,
	[24] = VMX_INT_TYPE_HW_EXP,
	[25] = VMX_INT_TYPE_HW_EXP,
	[26] = VMX_INT_TYPE_HW_EXP,
	[27] = VMX_INT_TYPE_HW_EXP,
	[28] = VMX_INT_TYPE_HW_EXP,
	[29] = VMX_INT_TYPE_HW_EXP,
	[30] = VMX_INT_TYPE_HW_EXP,
	[31] = VMX_INT_TYPE_HW_EXP
};

static int is_guest_irq_enabled(struct vcpu *vcpu)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];
	uint32_t guest_rflags, guest_state;
	int status = false;

	/* Read the RFLAGS of the guest */
	guest_rflags = cur_context->rflags;
	/* Check the RFLAGS[IF] bit first */
	if (guest_rflags & HV_ARCH_VCPU_RFLAGS_IF) {
		/* Interrupts are allowed */
		/* Check for temporarily disabled interrupts */
		guest_state = exec_vmread(VMX_GUEST_INTERRUPTIBILITY_INFO);

		if ((guest_state & (HV_ARCH_VCPU_BLOCKED_BY_STI |
				    HV_ARCH_VCPU_BLOCKED_BY_MOVSS)) == 0) {
			status = true;
		}
	}
	return status;
}

static bool vcpu_pending_request(struct vcpu *vcpu)
{
	struct vlapic *vlapic;
	int vector = 0;
	int ret = 0;

	/* Query vLapic to get vector to inject */
	vlapic = vcpu->arch_vcpu.vlapic;
	ret = vlapic_pending_intr(vlapic, &vector);

	/* we need to check and raise request if we have pending event
	 * in LAPIC IRR
	 */
	if (ret != 0) {
		/* we have pending IRR */
		vcpu_make_request(vcpu, ACRN_REQUEST_EVENT);
	}

	return vcpu->arch_vcpu.pending_intr != 0;
}

int vcpu_make_request(struct vcpu *vcpu, int eventid)
{
	bitmap_set(eventid, &vcpu->arch_vcpu.pending_intr);
	/*
	 * if current hostcpu is not the target vcpu's hostcpu, we need
	 * to invoke IPI to wake up target vcpu
	 *
	 * TODO: Here we just compare with cpuid, since cpuid currently is
	 *  global under pCPU / vCPU 1:1 mapping. If later we enabled vcpu
	 *  scheduling, we need change here to determine it target vcpu is
	 *  VMX non-root or root mode
	 */
	if ((int)get_cpu_id() != vcpu->pcpu_id)
		send_single_ipi(vcpu->pcpu_id, VECTOR_NOTIFY_VCPU);

	return 0;
}

static int vcpu_do_pending_event(struct vcpu *vcpu)
{
	struct vlapic *vlapic = vcpu->arch_vcpu.vlapic;
	int vector = 0;
	int ret = 0;

	if (is_vapic_intr_delivery_supported()) {
		apicv_inject_pir(vlapic);
		return 0;
	}

	/* Query vLapic to get vector to inject */
	ret = vlapic_pending_intr(vlapic, &vector);

	/*
	 * From the Intel SDM, Volume 3, 6.3.2 Section "Maskable
	 * Hardware Interrupts":
	 * - maskable interrupt vectors [16,255] can be delivered
	 *   through the local APIC.
	 */
	if (ret == 0)
		return -1;

	if (!(vector >= 16 && vector <= 255)) {
		dev_dbg(ACRN_DBG_INTR, "invalid vector %d from local APIC",
				vector);
		return -1;
	}

	exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD, VMX_INT_INFO_VALID |
		(vector & 0xFF));

	vlapic_intr_accepted(vlapic, vector);
	return 0;
}

static int vcpu_do_pending_extint(struct vcpu *vcpu)
{
	struct vm *vm;
	struct vcpu *primary;
	int vector;

	vm = vcpu->vm;

	/* check if there is valid interrupt from vPIC, if yes just inject it */
	/* PIC only connect with primary CPU */
	primary = get_primary_vcpu(vm);
	if (vm->vpic && vcpu == primary) {

		vpic_pending_intr(vcpu->vm, &vector);
		if (vector > 0) {
			dev_dbg(ACRN_DBG_INTR, "VPIC: to inject PIC vector %d\n",
					vector & 0xFF);
			exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
					VMX_INT_INFO_VALID |
					(vector & 0xFF));
			vpic_intr_accepted(vcpu->vm, vector);
		}
	}

	return 0;
}

static int vcpu_do_pending_gp(__unused struct vcpu *vcpu)
{
	/* SDM Vol. 3A 6-37: if the fault condition was detected while loading
	 * a segment descriptor, the error code contains a segment selecor to or
	 * IDT vector number for the descriptor; otherwise the error code is 0.
	 * Since currently there is no such case to inject #GP due to loading a
	 * segment decriptor, set the error code to 0.
	 */
	exec_vmwrite(VMX_ENTRY_EXCEPTION_ERROR_CODE, 0);
	/* GP vector = 13 */
	exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
		VMX_INT_INFO_VALID |
		((VMX_INT_TYPE_HW_EXP | EXCEPTION_ERROR_CODE_VALID) <<8) | 13);
	return 0;
}

/* please keep this for interrupt debug:
 * 1. Timer alive or not
 * 2. native LAPIC interrupt pending/EOI status
 * 3. CPU stuck or not
 */
void dump_lapic(void)
{
	dev_dbg(ACRN_DBG_INTR,
		"LAPIC: TIME %08x, init=0x%x cur=0x%x ISR=0x%x IRR=0x%x",
		mmio_read_long(HPA2HVA(LAPIC_BASE + LAPIC_LVT_TIMER_REGISTER)),
		mmio_read_long(HPA2HVA(LAPIC_BASE + LAPIC_INITIAL_COUNT_REGISTER)),
		mmio_read_long(HPA2HVA(LAPIC_BASE + LAPIC_CURRENT_COUNT_REGISTER)),
		mmio_read_long(HPA2HVA(LAPIC_BASE + LAPIC_IN_SERVICE_REGISTER_7)),
		mmio_read_long(HPA2HVA(LAPIC_BASE + LAPIC_INT_REQUEST_REGISTER_7)));
}

int vcpu_inject_extint(struct vcpu *vcpu)
{
	return vcpu_make_request(vcpu, ACRN_REQUEST_EXTINT);
}

int vcpu_inject_nmi(struct vcpu *vcpu)
{
	return vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
}

int vcpu_inject_gp(struct vcpu *vcpu)
{
	return vcpu_make_request(vcpu, ACRN_REQUEST_GP);
}

int interrupt_window_vmexit_handler(struct vcpu *vcpu)
{
	int value32;

	TRACE_2L(TRC_VMEXIT_INTERRUPT_WINDOW, 0, 0);

	if (!vcpu)
		return -1;

	if (vcpu_pending_request(vcpu)) {
		/* Do nothing
		 * acrn_do_intr_process will continue for this vcpu
		 */
	} else {
		/* No interrupts to inject.
		 * Disable the interrupt window exiting
		 */
		vcpu->arch_vcpu.irq_window_enabled = 0;
		value32 = exec_vmread(VMX_PROC_VM_EXEC_CONTROLS);
		value32 &= ~(VMX_PROCBASED_CTLS_IRQ_WIN);
		exec_vmwrite(VMX_PROC_VM_EXEC_CONTROLS, value32);
	}

	VCPU_RETAIN_RIP(vcpu);
	return 0;
}

int external_interrupt_vmexit_handler(struct vcpu *vcpu)
{
	uint32_t intr_info;
	struct intr_ctx ctx;

	intr_info = exec_vmread(VMX_EXIT_INT_INFO);
	if ((!(intr_info & VMX_INT_INFO_VALID)) ||
		(((intr_info & VMX_INT_TYPE_MASK) >> 8)
		!= VMX_INT_TYPE_EXT_INT)) {
		pr_err("Invalid VM exit interrupt info:%x", intr_info);
		VCPU_RETAIN_RIP(vcpu);
		return -EINVAL;
	}

	ctx.vector = intr_info & 0xFF;

	dispatch_interrupt(&ctx);

	VCPU_RETAIN_RIP(vcpu);

	TRACE_2L(TRC_VMEXIT_EXTERNAL_INTERRUPT, ctx.vector, 0);

	return 0;
}

int acrn_do_intr_process(struct vcpu *vcpu)
{
	int ret = 0;
	int vector;
	int tmp;
	bool intr_pending = false;
	uint64_t *pending_intr_bits = &vcpu->arch_vcpu.pending_intr;

	if (bitmap_test_and_clear(ACRN_REQUEST_TLB_FLUSH, pending_intr_bits))
		invept(vcpu);

	if (bitmap_test_and_clear(ACRN_REQUEST_TMR_UPDATE, pending_intr_bits))
		vioapic_update_tmr(vcpu);

	/* handling cancelled event injection when vcpu is switched out */
	if (vcpu->arch_vcpu.inject_event_pending) {
		exec_vmwrite(VMX_ENTRY_EXCEPTION_ERROR_CODE,
			vcpu->arch_vcpu.inject_info.error_code);

		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
			vcpu->arch_vcpu.inject_info.intr_info);

		vcpu->arch_vcpu.inject_event_pending = false;
		goto INTR_WIN;
	}

	/* handling pending vector injection:
	 * there are many reason inject failed, we need re-inject again
	 */
	if (vcpu->arch_vcpu.idt_vectoring_info & VMX_INT_INFO_VALID) {
		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
				vcpu->arch_vcpu.idt_vectoring_info);
		goto INTR_WIN;
	}

	/* handling exception request */
	vector = vcpu->arch_vcpu.exception_info.exception;

	/* If there is a valid exception, inject exception to guest */
	if (vector >= 0) {
		if (exception_type[vector] &
			EXCEPTION_ERROR_CODE_VALID) {
			exec_vmwrite(VMX_ENTRY_EXCEPTION_ERROR_CODE,
				vcpu->arch_vcpu.exception_info.error);
		}

		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
			VMX_INT_INFO_VALID |
			(exception_type[vector] << 8) | (vector & 0xFF));

		vcpu->arch_vcpu.exception_info.exception = -1;

		goto INTR_WIN;
	}

	/* Do pending interrupts process */
	/* TODO: checkin NMI intr windows before inject */
	if (bitmap_test_and_clear(ACRN_REQUEST_NMI, pending_intr_bits)) {
		/* Inject NMI vector = 2 */
		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
			VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8) | 2);

		/* Intel SDM 10.8.1
		 * NMI, SMI, INIT, ExtINT, or SIPI directly deliver to CPU
		 * do not need EOI to LAPIC
		 * However, ExtINT need EOI to PIC
		 */
		goto INTR_WIN;
	}

	/* Guest interruptable or not */
	if (is_guest_irq_enabled(vcpu)) {
		/* Inject external interrupt first */
		if (bitmap_test_and_clear(ACRN_REQUEST_EXTINT,
			pending_intr_bits)) {
			/* has pending external interrupts */
			ret = vcpu_do_pending_extint(vcpu);
			goto INTR_WIN;
		}

		/* Inject vLAPIC vectors */
		if (bitmap_test_and_clear(ACRN_REQUEST_EVENT,
			pending_intr_bits)) {
			/* has pending vLAPIC interrupts */
			ret = vcpu_do_pending_event(vcpu);
			goto INTR_WIN;
		}
	}

	/* Inject GP event */
	if (bitmap_test_and_clear(ACRN_REQUEST_GP, pending_intr_bits)) {
		/* has pending GP interrupts */
		ret = vcpu_do_pending_gp(vcpu);
		goto INTR_WIN;
	}

INTR_WIN:
	/* check if we have new interrupt pending for next VMExit */
	intr_pending = vcpu_pending_request(vcpu);

	/* Enable interrupt window exiting if pending */
	if (intr_pending && vcpu->arch_vcpu.irq_window_enabled == 0) {
		vcpu->arch_vcpu.irq_window_enabled = 1;
		tmp = exec_vmread(VMX_PROC_VM_EXEC_CONTROLS);
		tmp |= (VMX_PROCBASED_CTLS_IRQ_WIN);
		exec_vmwrite(VMX_PROC_VM_EXEC_CONTROLS, tmp);
	}

	return ret;
}

void cancel_event_injection(struct vcpu *vcpu)
{
	uint32_t intinfo;

	intinfo = exec_vmread(VMX_ENTRY_INT_INFO_FIELD);

	/*
	 * If event is injected, we clear VMX_ENTRY_INT_INFO_FIELD,
	 * save injection info, and mark inject event pending.
	 * The event will be re-injected in next acrn_do_intr_process
	 * call.
	 */
	if (intinfo & VMX_INT_INFO_VALID) {
		vcpu->arch_vcpu.inject_event_pending = true;

		if (intinfo & (EXCEPTION_ERROR_CODE_VALID << 8))
			vcpu->arch_vcpu.inject_info.error_code =
				exec_vmread(VMX_ENTRY_EXCEPTION_ERROR_CODE);

		vcpu->arch_vcpu.inject_info.intr_info = intinfo;
		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD, 0);
	}
}

int exception_vmexit_handler(struct vcpu *vcpu)
{
	uint32_t intinfo, int_err_code = 0;
	int32_t exception_vector = -1;
	uint32_t cpl;
	int status = 0;

	if (vcpu == NULL) {
		TRACE_4I(TRC_VMEXIT_EXCEPTION_OR_NMI, 0, 0, 0, 0);
		status = -EINVAL;
	}

	if (status != 0)
		return status;

	pr_dbg(" Handling guest exception");

	/* Obtain VM-Exit information field pg 2912 */
	intinfo = exec_vmread(VMX_EXIT_INT_INFO);
	if (intinfo & VMX_INT_INFO_VALID) {
		exception_vector = intinfo & 0xFF;
		/* Check if exception caused by the guest is a HW exception.
		 * If the exit occurred due to a HW exception obtain the
		 * error code to be conveyed to get via the stack
		 */
		if (intinfo & VMX_INT_INFO_ERR_CODE_VALID) {
			int_err_code = exec_vmread(VMX_EXIT_INT_ERROR_CODE);

			/* get current privilege level and fault address */
			cpl = exec_vmread(VMX_GUEST_CS_ATTR);
			cpl = (cpl >> 5) & 3;

			if (cpl < 3)
				int_err_code &= ~4;
			else
				int_err_code |= 4;
		}
	}

	/* Handle all other exceptions */
	VCPU_RETAIN_RIP(vcpu);
	vcpu->arch_vcpu.exception_info.exception = exception_vector;
	vcpu->arch_vcpu.exception_info.error = int_err_code;

	TRACE_4I(TRC_VMEXIT_EXCEPTION_OR_NMI,
			exception_vector, int_err_code, 2, 0);

	return status;
}
