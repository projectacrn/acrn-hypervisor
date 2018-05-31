/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#define EXCEPTION_ERROR_CODE_VALID  8
#define INTERRPUT_QUEUE_BUFF_SIZE   255

#define ACRN_DBG_INTR	6

#define EXCEPTION_CLASS_BENIGN	1
#define EXCEPTION_CLASS_CONT	2
#define EXCEPTION_CLASS_PF	3

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

	return vcpu->arch_vcpu.pending_req != 0;
}

int vcpu_make_request(struct vcpu *vcpu, int eventid)
{
	bitmap_set(eventid, &vcpu->arch_vcpu.pending_req);
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

/* SDM Vol3 -6.15, Table 6-4 - interrupt and exception classes */
static int get_excep_class(int32_t vector)
{
	if (vector == IDT_DE || vector == IDT_TS || vector == IDT_NP ||
		vector == IDT_SS || vector == IDT_GP)
		return EXCEPTION_CLASS_CONT;
	else if (vector == IDT_PF || vector == IDT_VE)
		return EXCEPTION_CLASS_PF;
	else
		return EXCEPTION_CLASS_BENIGN;
}

int vcpu_queue_exception(struct vcpu *vcpu, int32_t vector,
	uint32_t err_code)
{
	if (vector >= 32) {
		pr_err("invalid exception vector %d", vector);
		return -EINVAL;
	}

	if (vcpu->arch_vcpu.exception_info.exception >= 0) {
		int32_t prev_vector =
			vcpu->arch_vcpu.exception_info.exception;
		int32_t new_class, prev_class;

		/* SDM vol3 - 6.15, Table 6-5 - conditions for generating a
		 * double fault */
		prev_class = get_excep_class(prev_vector);
		new_class = get_excep_class(vector);
		if (prev_vector == IDT_DF &&
			new_class != EXCEPTION_CLASS_BENIGN) {
			/* triple fault happen - shutdwon mode */
			return vcpu_make_request(vcpu, ACRN_REQUEST_TRP_FAULT);
		} else if ((prev_class == EXCEPTION_CLASS_CONT &&
				new_class == EXCEPTION_CLASS_CONT) ||
				(prev_class == EXCEPTION_CLASS_PF &&
				 new_class != EXCEPTION_CLASS_BENIGN)) {
			/* generate double fault */
			vector = IDT_DF;
			err_code = 0;
		}
	}

	vcpu->arch_vcpu.exception_info.exception = vector;

	if (exception_type[vector] & EXCEPTION_ERROR_CODE_VALID)
		vcpu->arch_vcpu.exception_info.error = err_code;
	else
		vcpu->arch_vcpu.exception_info.error = 0;

	return 0;
}

static void _vcpu_inject_exception(struct vcpu *vcpu, uint32_t vector)
{
	if (exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) {
		exec_vmwrite(VMX_ENTRY_EXCEPTION_ERROR_CODE,
				vcpu->arch_vcpu.exception_info.error);
	}

	exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD, VMX_INT_INFO_VALID |
			(exception_type[vector] << 8) | (vector & 0xFF));

	vcpu->arch_vcpu.exception_info.exception = -1;
}

static int vcpu_inject_hi_exception(struct vcpu *vcpu)
{
	int vector = vcpu->arch_vcpu.exception_info.exception;

	if (vector == IDT_MC || vector == IDT_BP || vector == IDT_DB) {
		_vcpu_inject_exception(vcpu, vector);
		return 1;
	}

	return 0;
}

static int vcpu_inject_lo_exception(struct vcpu *vcpu)
{
	int vector = vcpu->arch_vcpu.exception_info.exception;

	/* high priority exception already be injected */
	if (vector >= 0) {
		_vcpu_inject_exception(vcpu, vector);
		return 1;
	}

	return 0;
}

int vcpu_inject_extint(struct vcpu *vcpu)
{
	return vcpu_make_request(vcpu, ACRN_REQUEST_EXTINT);
}

int vcpu_inject_nmi(struct vcpu *vcpu)
{
	return vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
}

int vcpu_inject_gp(struct vcpu *vcpu, uint32_t err_code)
{
	vcpu_queue_exception(vcpu, IDT_GP, err_code);
	return vcpu_make_request(vcpu, ACRN_REQUEST_EXCP);
}

int vcpu_inject_pf(struct vcpu *vcpu, uint64_t addr, uint32_t err_code)
{
	struct run_context *cur_context =
		&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	cur_context->cr2 = addr;
	vcpu_queue_exception(vcpu, IDT_PF, err_code);
	return vcpu_make_request(vcpu, ACRN_REQUEST_EXCP);
}

int interrupt_window_vmexit_handler(struct vcpu *vcpu)
{
	int value32;

	TRACE_2L(TRC_VMEXIT_INTERRUPT_WINDOW, 0, 0);

	if (!vcpu)
		return -1;

	if (vcpu_pending_request(vcpu)) {
		/* Do nothing
		 * acrn_handle_pending_request will continue for this vcpu
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
	struct intr_excp_ctx ctx;

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

int acrn_handle_pending_request(struct vcpu *vcpu)
{
	int ret = 0;
	int tmp;
	bool intr_pending = false;
	uint64_t *pending_req_bits = &vcpu->arch_vcpu.pending_req;

	if (bitmap_test_and_clear(ACRN_REQUEST_TRP_FAULT, pending_req_bits)) {
		pr_fatal("Triple fault happen -> shutdown!");
		return -EFAULT;
	}

	if (bitmap_test_and_clear(ACRN_REQUEST_EPT_FLUSH, pending_req_bits))
		invept(vcpu);

	if (bitmap_test_and_clear(ACRN_REQUEST_VPID_FLUSH, pending_req_bits))
		flush_vpid_single(vcpu->arch_vcpu.vpid);

	if (bitmap_test_and_clear(ACRN_REQUEST_TMR_UPDATE, pending_req_bits))
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

	/* SDM Vol 3 - table 6-2, inject high priority exception before
	 * maskable hardware interrupt */
	if (vcpu_inject_hi_exception(vcpu))
		goto INTR_WIN;

	/* inject NMI before maskable hardware interrupt */
	if (bitmap_test_and_clear(ACRN_REQUEST_NMI, pending_req_bits)) {
		/* Inject NMI vector = 2 */
		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
			VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8) | IDT_NMI);

		goto INTR_WIN;
	}

	/* handling pending vector injection:
	 * there are many reason inject failed, we need re-inject again
	 * here should take care
	 * - SW exception (not maskable by IF)
	 * - external interrupt, if IF clear, will keep in IDT_VEC_INFO_FIELD
	 *   at next vm exit?
	 */
	if (vcpu->arch_vcpu.idt_vectoring_info & VMX_INT_INFO_VALID) {
		exec_vmwrite(VMX_ENTRY_INT_INFO_FIELD,
				vcpu->arch_vcpu.idt_vectoring_info);
		goto INTR_WIN;
	}

	/* Guest interruptable or not */
	if (is_guest_irq_enabled(vcpu)) {
		/* Inject external interrupt first */
		if (bitmap_test_and_clear(ACRN_REQUEST_EXTINT,
			pending_req_bits)) {
			/* has pending external interrupts */
			ret = vcpu_do_pending_extint(vcpu);
			goto INTR_WIN;
		}

		/* Inject vLAPIC vectors */
		if (bitmap_test_and_clear(ACRN_REQUEST_EVENT,
			pending_req_bits)) {
			/* has pending vLAPIC interrupts */
			ret = vcpu_do_pending_event(vcpu);
			goto INTR_WIN;
		}
	}

	/* SDM Vol3 table 6-2, inject lowpri exception */
	if (vcpu_inject_lo_exception(vcpu))
		goto INTR_WIN;

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
	 * The event will be re-injected in next acrn_handle_pending_request
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

	vcpu_queue_exception(vcpu, exception_vector, int_err_code);

	if (exception_vector == IDT_MC) {
		/* just print error message for #MC, it then will be injected
		 * back to guest */
		pr_fatal("Exception #MC got from guest!");
	}

	TRACE_4I(TRC_VMEXIT_EXCEPTION_OR_NMI,
			exception_vector, int_err_code, 2, 0);

	return status;
}
