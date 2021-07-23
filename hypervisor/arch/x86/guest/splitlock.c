/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/virq.h>
#include <event.h>
#include <asm/cpu_caps.h>
#include <logmsg.h>
#include <errno.h>
#include <asm/guest/splitlock.h>

static bool is_guest_ac_enabled(struct acrn_vcpu *vcpu)
{
	bool ret = false;

	if ((vcpu_get_guest_msr(vcpu, MSR_TEST_CTL) & MSR_TEST_CTL_AC_SPLITLOCK) != 0UL) {
		ret = true;
	}

	return ret;
}

static bool is_guest_gp_enabled(struct acrn_vcpu *vcpu)
{
	bool ret = false;

	if ((vcpu_get_guest_msr(vcpu, MSR_TEST_CTL) & MSR_TEST_CTL_GP_UCLOCK) != 0UL) {
		ret = true;
	}

	return ret;
}

void vcpu_kick_splitlock_emulation(struct acrn_vcpu *cur_vcpu)
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

void vcpu_complete_splitlock_emulation(struct acrn_vcpu *cur_vcpu)
{
	struct acrn_vcpu *other;
	uint16_t i;

	if (cur_vcpu->vm->hw.created_vcpus > 1U) {
		foreach_vcpu(i, cur_vcpu->vm, other) {
			if (other != cur_vcpu) {
				signal_event(&other->events[VCPU_EVENT_SPLIT_LOCK]);
			}
		}

		put_vm_lock(cur_vcpu->vm);
	}
}

int32_t emulate_splitlock(struct acrn_vcpu *vcpu, uint32_t exception_vector, bool *queue_exception)
{
	int32_t status = 0;
	uint8_t inst[1];
	uint32_t err_code = 0U;
	uint64_t fault_addr;

	/* Queue the exception by default if the exception cannot be handled. */
	*queue_exception = true;

	/*
	 * The split-lock/uc-lock detection is enabled by default if the platform supports it.
	 * Here, we check if the split-lock detection is really enabled or not. If the
	 * split-lock/uc-lock detection is enabled in the platform but not enabled in the guest
	 * then we try to emulate it, otherwise, inject the exception back.
	 */
	if ((is_ac_enabled() && !is_guest_ac_enabled(vcpu)) || (is_gp_enabled() && !is_guest_gp_enabled(vcpu))){
		switch (exception_vector) {
		case IDT_AC:
		case IDT_GP:
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
				 * If #AC/#GP is caused by instruction with LOCK prefix or xchg, then emulate it,
				 * otherwise, inject it back.
				 */
				if (inst[0] == 0xf0U) {  /* This is LOCK prefix */
					/*
					 * Kick other vcpus of the guest to stop execution
					 * until the split-lock/uc-lock emulation being completed.
					 */
					vcpu_kick_splitlock_emulation(vcpu);

					/*
					 * Skip the LOCK prefix and re-execute the instruction.
					 */
					vcpu->arch.inst_len = 1U;
					if (vcpu->vm->hw.created_vcpus > 1U) {
						/* Enable MTF to start single-stepping execution */
						vcpu->arch.proc_vm_exec_ctrls |= VMX_PROCBASED_CTLS_MON_TRAP;
						exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, vcpu->arch.proc_vm_exec_ctrls);
						vcpu->arch.emulating_lock = true;
					}

					/* Skip the #AC/#GP, we have emulated it. */
					*queue_exception = false;
				} else {
					status = decode_instruction(vcpu, false);
					if (status >= 0) {
						/*
						 * If this is the xchg, then emulate it, otherwise,
						 * inject it back.
						 */
						if (is_current_opcode_xchg(vcpu)) {
							/*
							 * Kick other vcpus of the guest to stop execution
							 * until the split-lock/uc-lock emulation being completed.
							 */
							vcpu_kick_splitlock_emulation(vcpu);

							/*
							 * Using emulating_lock to make sure xchg emulation
							 * is only called by split-lock/uc-lock emulation.
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

							/* Do not inject #AC/#GP, we have emulated it */
							*queue_exception = false;
						}
					} else {
						if (status == -EFAULT) {
							pr_info("page fault happen during decode_instruction");
							/* For this case, Inject #PF, not to queue #AC/#GP */
							*queue_exception = false;
						}

						/* if decode_instruction(full_decode = false) return -1, that means this is an unknown instruction,
						 * and has skipped #UD injection. Just keep queue_exception = true to inject #AC back */
						status = 0;
					}
				}
			}

			break;
		default:
			break;
		}
	}

	return status;
}
