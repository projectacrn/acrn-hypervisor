/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <irq.h>
#include <lapic.h>
#include <mmu.h>
#include <vmx.h>
#include <vcpu.h>
#include <vmcs.h>
#include <vm.h>
#include <trace.h>
#include <logmsg.h>

#define EXCEPTION_ERROR_CODE_VALID  8U

#define DBG_LEVEL_INTR	6U

#define EXCEPTION_CLASS_BENIGN	1
#define EXCEPTION_CLASS_CONT	2
#define EXCEPTION_CLASS_PF	3

/* Exception types */
#define EXCEPTION_FAULT		0U
#define EXCEPTION_TRAP		1U
#define EXCEPTION_ABORT		2U
#define EXCEPTION_INTERRUPT	3U

static const uint16_t exception_type[32] = {
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

static uint8_t get_exception_type(uint32_t vector)
{
	uint8_t type;

	/* Treat #DB as trap until decide to support Debug Registers */
	if ((vector > 31U) || (vector == IDT_NMI)) {
		type = EXCEPTION_INTERRUPT;
	} else if ((vector == IDT_DB) || (vector == IDT_BP) || (vector ==  IDT_OF)) {
		type = EXCEPTION_TRAP;
	} else if ((vector == IDT_DF) || (vector == IDT_MC)) {
		type = EXCEPTION_ABORT;
	} else {
		type = EXCEPTION_FAULT;
	}

	return type;
}

static bool is_guest_irq_enabled(struct acrn_vcpu *vcpu)
{
	uint64_t guest_rflags, guest_state;
	bool status = false;

	/* Read the RFLAGS of the guest */
	guest_rflags = vcpu_get_rflags(vcpu);
	/* Check the RFLAGS[IF] bit first */
	if ((guest_rflags & HV_ARCH_VCPU_RFLAGS_IF) != 0UL) {
		/* Interrupts are allowed */
		/* Check for temporarily disabled interrupts */
		guest_state = exec_vmread32(VMX_GUEST_INTERRUPTIBILITY_INFO);

		if ((guest_state & (HV_ARCH_VCPU_BLOCKED_BY_STI |
				    HV_ARCH_VCPU_BLOCKED_BY_MOVSS)) == 0UL) {
			status = true;
		}
	}
	return status;
}

static inline bool is_nmi_injectable(void)
{
	uint64_t guest_state;

	guest_state = exec_vmread32(VMX_GUEST_INTERRUPTIBILITY_INFO);

	return ((guest_state & (HV_ARCH_VCPU_BLOCKED_BY_STI |
		HV_ARCH_VCPU_BLOCKED_BY_MOVSS | HV_ARCH_VCPU_BLOCKED_BY_NMI)) == 0UL);
}
void vcpu_make_request(struct acrn_vcpu *vcpu, uint16_t eventid)
{
	bitmap_set_lock(eventid, &vcpu->arch.pending_req);
	kick_vcpu(vcpu);
}

/*
 * @retval true when INT is injected to guest.
 * @retval false when otherwise
 */
static bool vcpu_do_pending_extint(const struct acrn_vcpu *vcpu)
{
	struct acrn_vm *vm;
	struct acrn_vcpu *primary;
	uint32_t vector;
	bool ret = false;

	vm = vcpu->vm;

	/* check if there is valid interrupt from vPIC, if yes just inject it */
	/* PIC only connect with primary CPU */
	primary = vcpu_from_vid(vm, BSP_CPU_ID);
	if (vcpu == primary) {

		vpic_pending_intr(vm_pic(vcpu->vm), &vector);
		if (vector <= NR_MAX_VECTOR) {
			dev_dbg(DBG_LEVEL_INTR, "VPIC: to inject PIC vector %d\n",
					vector & 0xFFU);
			exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD,
					VMX_INT_INFO_VALID |
					(vector & 0xFFU));
			vpic_intr_accepted(vm_pic(vcpu->vm), vector);
			ret = true;
		}
	}

	return ret;
}

/* SDM Vol3 -6.15, Table 6-4 - interrupt and exception classes */
static int32_t get_excep_class(uint32_t vector)
{
	int32_t ret;

	if ((vector == IDT_DE) || (vector == IDT_TS) || (vector == IDT_NP) ||
		(vector == IDT_SS) || (vector == IDT_GP)) {
		ret = EXCEPTION_CLASS_CONT;
	} else if ((vector == IDT_PF) || (vector == IDT_VE)) {
		ret = EXCEPTION_CLASS_PF;
	} else {
		ret = EXCEPTION_CLASS_BENIGN;
	}

	return ret;
}

int32_t vcpu_queue_exception(struct acrn_vcpu *vcpu, uint32_t vector_arg, uint32_t err_code_arg)
{
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	uint32_t vector = vector_arg;
	uint32_t err_code = err_code_arg;
	int32_t ret = 0;

	/* VECTOR_INVALID is also greater than 32 */
	if (vector >= 32U) {
		pr_err("invalid exception vector %d", vector);
		ret = -EINVAL;
	} else {

		uint32_t prev_vector = arch->exception_info.exception;
		int32_t new_class, prev_class;

		/* SDM vol3 - 6.15, Table 6-5 - conditions for generating a
		 * double fault */
		prev_class = get_excep_class(prev_vector);
		new_class = get_excep_class(vector);
		if ((prev_vector == IDT_DF) && (new_class != EXCEPTION_CLASS_BENIGN)) {
			/* triple fault happen - shutdwon mode */
			vcpu_make_request(vcpu, ACRN_REQUEST_TRP_FAULT);
		} else {
			if (((prev_class == EXCEPTION_CLASS_CONT) && (new_class == EXCEPTION_CLASS_CONT)) ||
				((prev_class == EXCEPTION_CLASS_PF) && (new_class != EXCEPTION_CLASS_BENIGN))) {
				/* generate double fault */
				vector = IDT_DF;
				err_code = 0U;
			} else {
				/* Trigger the given exception instead of override it with
				 * double/triple fault. */
			}

			arch->exception_info.exception = vector;

			if ((exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) != 0U) {
				arch->exception_info.error = err_code;
			} else {
				arch->exception_info.error = 0U;
			}

			if ((vector == IDT_NMI) && is_notification_nmi(vcpu->vm)) {
				/*
				 * Currently, ACRN doesn't support vNMI well and there is no well-designed
				 * way to check if the NMI is for notification or not. Here we take all the
				 * NMIs as notification NMI for lapic-pt VMs temporarily.
				 *
				 * TODO: Add a way in is_notification_nmi to check the NMI is for notification
				 *       or not in order to support vNMI.
				 */
				pr_dbg("This NMI is used as notification signal. So ignore it.");
			} else {
				vcpu_make_request(vcpu, ACRN_REQUEST_EXCP);
			}
		}
	}

	return ret;
}

/*
 * @pre vcpu->arch.exception_info.exception < 0x20U
 */
static bool vcpu_inject_exception(struct acrn_vcpu *vcpu)
{
	bool injected = false;

	if (bitmap_test_and_clear_lock(ACRN_REQUEST_EXCP, &vcpu->arch.pending_req)) {
		uint32_t vector = vcpu->arch.exception_info.exception;
	
		if ((exception_type[vector] & EXCEPTION_ERROR_CODE_VALID) != 0U) {
			exec_vmwrite32(VMX_ENTRY_EXCEPTION_ERROR_CODE,
					vcpu->arch.exception_info.error);
		}

		exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, VMX_INT_INFO_VALID |
				(exception_type[vector] << 8U) | (vector & 0xFFU));

		vcpu->arch.exception_info.exception = VECTOR_INVALID;

		/* If this is a fault, we should retain the RIP */
		if (get_exception_type(vector) == EXCEPTION_FAULT) {
			vcpu_retain_rip(vcpu);
		}

		/* SDM 17.3.1.1 For any fault-class exception except a debug exception generated in response to an
		 * instruction breakpoint, the value pushed for RF is 1.
		 * #DB is treated as Trap in get_exception_type, so RF will not be set for instruction breakpoint.
		 */
		if (get_exception_type(vector) == EXCEPTION_FAULT) {
			vcpu_set_rflags(vcpu, vcpu_get_rflags(vcpu) | HV_ARCH_VCPU_RFLAGS_RF);
		}

		injected = true;
	}

	return injected;
}

/* Inject external interrupt to guest */
void vcpu_inject_extint(struct acrn_vcpu *vcpu)
{
	vcpu_make_request(vcpu, ACRN_REQUEST_EXTINT);
	signal_event(&vcpu->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
}

/* Inject NMI to guest */
void vcpu_inject_nmi(struct acrn_vcpu *vcpu)
{
	vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
	signal_event(&vcpu->events[VCPU_EVENT_VIRTUAL_INTERRUPT]);
}

/* Inject general protection exception(#GP) to guest */
void vcpu_inject_gp(struct acrn_vcpu *vcpu, uint32_t err_code)
{
	(void)vcpu_queue_exception(vcpu, IDT_GP, err_code);
}

/* Inject page fault exception(#PF) to guest */
void vcpu_inject_pf(struct acrn_vcpu *vcpu, uint64_t addr, uint32_t err_code)
{
	vcpu_set_cr2(vcpu, addr);
	(void)vcpu_queue_exception(vcpu, IDT_PF, err_code);
}

/* Inject invalid opcode exception(#UD) to guest */
void vcpu_inject_ud(struct acrn_vcpu *vcpu)
{
	(void)vcpu_queue_exception(vcpu, IDT_UD, 0);
}

/* Inject stack fault exception(#SS) to guest */
void vcpu_inject_ss(struct acrn_vcpu *vcpu)
{
	(void)vcpu_queue_exception(vcpu, IDT_SS, 0);
}

int32_t interrupt_window_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t value32;

	TRACE_2L(TRACE_VMEXIT_INTERRUPT_WINDOW, 0UL, 0UL);

	/* Disable interrupt-window exiting first.
	 * acrn_handle_pending_request will continue handle for this vcpu
	 */
	vcpu->arch.irq_window_enabled = false;
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
	value32 &= ~(VMX_PROCBASED_CTLS_IRQ_WIN);
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);

	vcpu_retain_rip(vcpu);
	return 0;
}

int32_t external_interrupt_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t intr_info;
	struct intr_excp_ctx ctx;
	int32_t ret;

	intr_info = exec_vmread32(VMX_EXIT_INT_INFO);
	if (((intr_info & VMX_INT_INFO_VALID) == 0U) ||
		(((intr_info & VMX_INT_TYPE_MASK) >> 8U)
		!= VMX_INT_TYPE_EXT_INT)) {
		pr_err("Invalid VM exit interrupt info:%x", intr_info);
		vcpu_retain_rip(vcpu);
		ret = -EINVAL;
	} else {
		ctx.vector = intr_info & 0xFFU;
		ctx.rip    = vcpu_get_rip(vcpu);
		ctx.rflags = vcpu_get_rflags(vcpu);
		ctx.cs     = exec_vmread32(VMX_GUEST_CS_SEL);

		dispatch_interrupt(&ctx);
		vcpu_retain_rip(vcpu);

		TRACE_2L(TRACE_VMEXIT_EXTERNAL_INTERRUPT, ctx.vector, 0UL);
		ret = 0;
	}

	return ret;
}

static inline void acrn_inject_pending_intr(struct acrn_vcpu *vcpu,
		uint64_t *pending_req_bits, bool injected);

int32_t acrn_handle_pending_request(struct acrn_vcpu *vcpu)
{
	bool injected = false;
	int32_t ret = 0;
	uint32_t tmp;
	struct acrn_vcpu_arch *arch = &vcpu->arch;
	uint64_t *pending_req_bits = &arch->pending_req;

	/* make sure ACRN_REQUEST_INIT_VMCS handler as the first one */
	if (bitmap_test_and_clear_lock(ACRN_REQUEST_INIT_VMCS, pending_req_bits)) {
		init_vmcs(vcpu);
	}

	if (bitmap_test_and_clear_lock(ACRN_REQUEST_TRP_FAULT, pending_req_bits)) {
		pr_fatal("Triple fault happen -> shutdown!");
		ret = -EFAULT;
	} else {
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_WAIT_WBINVD, pending_req_bits)) {
			wait_event(&vcpu->events[VCPU_EVENT_SYNC_WBINVD]);
		}

		if (bitmap_test_and_clear_lock(ACRN_REQUEST_SPLIT_LOCK, pending_req_bits)) {
			wait_event(&vcpu->events[VCPU_EVENT_SPLIT_LOCK]);
		}

		if (bitmap_test_and_clear_lock(ACRN_REQUEST_EPT_FLUSH, pending_req_bits)) {
			invept(vcpu->vm->arch_vm.nworld_eptp);
			if (vcpu->vm->sworld_control.flag.active != 0UL) {
				invept(vcpu->vm->arch_vm.sworld_eptp);
			}
		}

		if (bitmap_test_and_clear_lock(ACRN_REQUEST_VPID_FLUSH,	pending_req_bits)) {
			flush_vpid_single(arch->vpid);
		}

		if (bitmap_test_and_clear_lock(ACRN_REQUEST_EOI_EXIT_BITMAP_UPDATE, pending_req_bits)) {
			vcpu_set_vmcs_eoi_exit(vcpu);
		}

		/*
		 * Inject pending exception prior pending interrupt to complete the previous instruction.
		 */
		injected = vcpu_inject_exception(vcpu);
		if (!injected) {
			/* inject NMI before maskable hardware interrupt */

			if (bitmap_test_and_clear_lock(ACRN_REQUEST_NMI, pending_req_bits)) {
				if (is_nmi_injectable()) {
					/* Inject NMI vector = 2 */
					exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD,
							VMX_INT_INFO_VALID | (VMX_INT_TYPE_NMI << 8U) | IDT_NMI);
					injected = true;
				} else {
					/* keep the NMI request for next vmexit */
					bitmap_set_lock(ACRN_REQUEST_NMI, pending_req_bits);
				}
			} else {
				/* handling pending vector injection:
				 * there are many reason inject failed, we need re-inject again
				 * here should take care
				 * - SW exception (not maskable by IF)
				 * - external interrupt, if IF clear, will keep in IDT_VEC_INFO_FIELD
				 *   at next vm exit?
				 */
				if ((arch->idt_vectoring_info & VMX_INT_INFO_VALID) != 0U) {
					exec_vmwrite32(VMX_ENTRY_INT_INFO_FIELD, arch->idt_vectoring_info);
					arch->idt_vectoring_info = 0U;
					injected = true;
				}
			}
		}

		acrn_inject_pending_intr(vcpu, pending_req_bits, injected);

		/*
		 * If "virtual-interrupt delivered" is enabled, CPU will evaluate
		 * and automatic inject the virtual interrupts in appropriate time.
		 * And from SDM Vol3 29.2.1, the apicv only trigger evaluation of
		 * pending virtual interrupts when "interrupt-window exiting" is 0.
		 *
		 * External interrupt(from vpic) can't be delivered by "virtual-
		 * interrupt delivery", it only deliver interrupt from vlapic.
		 *
		 * So need to enable "interrupt-window exiting", when there is
		 * an ExtInt or there is lapic interrupt and virtual interrupt
		 * deliver is disabled.
		 */
		if (!arch->irq_window_enabled) {
			/*
			 * TODO: Currently, NMI exiting and virtual NMIs are not enabled,
			 * so use interrupt window to inject NMI.
			 * After enable virtual NMIs, we can use NMI-Window
			 */
			if (bitmap_test(ACRN_REQUEST_EXTINT, pending_req_bits) ||
				bitmap_test(ACRN_REQUEST_NMI, pending_req_bits) ||
				vlapic_has_pending_delivery_intr(vcpu)) {
				tmp = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
				tmp |= VMX_PROCBASED_CTLS_IRQ_WIN;
				exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, tmp);
				arch->irq_window_enabled = true;
			}
		}
	}

	return ret;
}

static inline void acrn_inject_pending_intr(struct acrn_vcpu *vcpu,
		uint64_t *pending_req_bits, bool injected)
{
	bool ret = injected;
	bool guest_irq_enabled = is_guest_irq_enabled(vcpu);

	if (guest_irq_enabled && (!ret)) {
		/* Inject external interrupt first */
		if (bitmap_test_and_clear_lock(ACRN_REQUEST_EXTINT, pending_req_bits)) {
			/* has pending external interrupts */
			ret = vcpu_do_pending_extint(vcpu);
		}
	}

	if (bitmap_test_and_clear_lock(ACRN_REQUEST_EVENT, pending_req_bits)) {
		vlapic_inject_intr(vcpu_vlapic(vcpu), guest_irq_enabled, ret);
	}
}

static void vcpu_kick_splitlock_emulation(struct acrn_vcpu *cur_vcpu)
{
	struct acrn_vcpu *other;
	uint16_t i;

	if (cur_vcpu->vm->hw.created_vcpus > 1U) {
		get_vm_lock(cur_vcpu->vm);

		foreach_vcpu(i, cur_vcpu->vm, other) {
			if (other != cur_vcpu) {
				vcpu_make_request(other, ACRN_REQUEST_SPLIT_LOCK);
			}
		}
	}
}

static void vcpu_complete_splitlock_emulation(struct acrn_vcpu *cur_vcpu)
{
	struct acrn_vcpu *other;
	uint16_t i;

	if (cur_vcpu->vm->hw.created_vcpus > 1U) {
		foreach_vcpu(i, cur_vcpu->vm, other) {
			if (other != cur_vcpu) {
				/*
				 * Suppose the current vcpu is 0, the other vcpus (1, 2, 3) may wait on the
				 * "get_vm_lock", the current vcpu need clear the ACRN_REQUEST_SPLIT_LOCK
				 * explicitly here after finishing the emulation. Otherwise, it make cause
				 * dead lock. for example:
				 * 	1. Once vcpu 0 "put_vm_lock", let's say vcpu 1 will "get_vm_lock".
				 *	2. vcpu 1 call "vcpu_make_request" to pause vcpu 0, 2, 3.
				 *	3. vcpu 1's VCPU_EVENT_SPLIT_LOCK is still not cleared because
				 *	   the vcpu 0 called "vcpu_make_request" ever.
				 *	4. All vcpus will wait for VCPU_EVENT_SPLIT_LOCK in acrn_handle_pending_request.
				 * We should avoid this dead lock case.
				 */
				bitmap_clear_lock(ACRN_REQUEST_SPLIT_LOCK, &other->arch.pending_req);
				signal_event(&other->events[VCPU_EVENT_SPLIT_LOCK]);
			}
		}

		put_vm_lock(cur_vcpu->vm);
	}
}

static bool is_guest_ac_enabled(struct acrn_vcpu *vcpu)
{
	bool ret = false;

	if ((vcpu_get_guest_msr(vcpu, MSR_TEST_CTL) & (1UL << 29UL)) != 0UL) {
		ret = true;
	}

	return ret;
}

static int32_t emulate_splitlock(struct acrn_vcpu *vcpu, uint32_t exception_vector, bool *queue_exception)
{
	int32_t status = 0;
	uint8_t inst[1];
	uint32_t err_code = 0U;
	uint64_t fault_addr;

	/* Queue the exception by default if the exception cannot be handled. */
	*queue_exception = true;

	/*
	 * The split-lock detection is enabled by default if the platform supports it.
	 * Here, we check if the split-lock detection is really enabled or not. If the
	 * split-lock detection is enabled in the platform but not enabled in the guest
	 * then we try to emulate it, otherwise, inject the exception back.
	 */
	if (is_ac_enabled() && !is_guest_ac_enabled(vcpu)) {
		switch (exception_vector) {
		case IDT_AC:
			status = copy_from_gva(vcpu, inst, vcpu_get_rip(vcpu), 1U, &err_code, &fault_addr);
			if (status < 0) {
				pr_err("Error copy instruction from Guest!");
				if (status == -EFAULT) {
					vcpu_inject_pf(vcpu, fault_addr, err_code);
					status = 0;
					/* For this case, inject #PF, not to queue #AC */
					*queue_exception = false;
				}
			} else {
				/*
				 * If AC is caused by instruction with LOCK prefix or xchg, then emulate it,
				 * otherwise, inject it back.
				 */
				if (inst[0] == 0xf0U) {  /* This is LOCK prefix */
					/*
					 * Kick other vcpus of the guest to stop execution
					 * until the split-lock emulation being completed.
					 */
					vcpu_kick_splitlock_emulation(vcpu);

					/*
					 * Skip the LOCK prefix and re-execute the instruction.
					 */
					vcpu->arch.inst_len = 1U;
					if (vcpu->vm->hw.created_vcpus > 1U) {
						/*
						 * Set the TF to have a #DB after running the split-lock
						 * instruction and tag the emulating_lock to be true.
						 */
						vcpu_set_rflags(vcpu, vcpu_get_rflags(vcpu) | HV_ARCH_VCPU_RFLAGS_TF);
						vcpu->arch.emulating_lock = true;
					}

					/* Skip the #AC, we have emulated it. */
					*queue_exception = false;
				} else {
					status = decode_instruction(vcpu);
					if (status >= 0) {
						/*
						 * If this is the xchg, then emulate it, otherwise,
						 * inject it back.
						 */
						if (is_current_opcode_xchg(vcpu)) {
							/*
							 * Kick other vcpus of the guest to stop execution
							 * until the split-lock emulation being completed.
							 */
							vcpu_kick_splitlock_emulation(vcpu);

							/*
							 * Using emulating_lock to make sure xchg emulation
							 * is only called by split-lock emulation.
							 */
							vcpu->arch.emulating_lock = true;
							status = emulate_instruction(vcpu);
							vcpu->arch.emulating_lock = false;
							if (status < 0) {
								if (status == -EFAULT) {
									pr_info("page fault happen during emulate_instruction");
									status = 0;
								}
							}

							/*
							 * Notify other vcpus of the guest to restart execution.
							 */
							vcpu_complete_splitlock_emulation(vcpu);

							/* Do not inject #AC, we have emulated it */
							*queue_exception = false;
						}
					} else {
						if (status == -EFAULT) {
							pr_info("page fault happen during decode_instruction");
							status = 0;
							/* For this case, Inject #PF, not to queue #AC */
							*queue_exception = false;
						}
					}
				}
			}

			break;
		case IDT_DB:
			/*
			 * We only handle #DB caused by split-lock emulation,
			 * otherwise, inject it back.
			 */
			if (vcpu->arch.emulating_lock) {
				/*
				 * The split-lock emulation has been completed, tag the emulating_lock
				 * to be false, and clear the TF flag.
				 */
				vcpu->arch.emulating_lock = false;
				vcpu_set_rflags(vcpu, vcpu_get_rflags(vcpu) & (~HV_ARCH_VCPU_RFLAGS_TF));
				/*
				 * Notify other vcpus of the guest to restart execution.
				 */
				vcpu_complete_splitlock_emulation(vcpu);

				/* This #DB is for split-lock emulation, do not inject it */
				*queue_exception = false;

				/* This case we should not skip the instruction */
				vcpu->arch.inst_len = 0U;
			}

			break;
		default:
			break;
		}
	}

	return status;
}

/*
 * @pre vcpu != NULL
 */
int32_t exception_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t intinfo, int_err_code = 0U;
	uint32_t exception_vector = VECTOR_INVALID;
	uint32_t cpl;
	int32_t status = 0;
	bool queue_exception;

	pr_dbg(" Handling guest exception");

	/* Obtain VM-Exit information field pg 2912 */
	intinfo = exec_vmread32(VMX_EXIT_INT_INFO);
	if ((intinfo & VMX_INT_INFO_VALID) != 0U) {
		exception_vector = intinfo & 0xFFU;
		/* Check if exception caused by the guest is a HW exception.
		 * If the exit occurred due to a HW exception obtain the
		 * error code to be conveyed to get via the stack
		 */
		if ((intinfo & VMX_INT_INFO_ERR_CODE_VALID) != 0U) {
			int_err_code = exec_vmread32(VMX_EXIT_INT_ERROR_CODE);

			/* get current privilege level and fault address */
			cpl = exec_vmread32(VMX_GUEST_CS_ATTR);
			cpl = (cpl >> 5U) & 3U;

			if (cpl < 3U) {
				int_err_code &= ~4U;
			} else {
				int_err_code |= 4U;
			}
		}
	}

	status = emulate_splitlock(vcpu, exception_vector, &queue_exception);
	if ((status == 0) && queue_exception) {
		vcpu_retain_rip(vcpu);
		status = vcpu_queue_exception(vcpu, exception_vector, int_err_code);
	}

	if (exception_vector == IDT_MC) {
		/* just print error message for #MC, it then will be injected
		 * back to guest */
		pr_fatal("Exception #MC got from guest!");
	}

	TRACE_4I(TRACE_VMEXIT_EXCEPTION_OR_NMI,
			exception_vector, int_err_code, 2U, 0U);

	return status;
}

int32_t nmi_window_vmexit_handler(struct acrn_vcpu *vcpu)
{
	uint32_t value32;

	/*
	 * Disable NMI-window exiting here. We will process
	 * the pending request in acrn_handle_pending_request later
	 */
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
	value32 &= ~VMX_PROCBASED_CTLS_NMI_WINEXIT;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);

	vcpu_retain_rip(vcpu);

	return 0;
}
