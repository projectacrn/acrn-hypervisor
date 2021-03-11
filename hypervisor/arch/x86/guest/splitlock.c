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

	if ((vcpu_get_guest_msr(vcpu, MSR_TEST_CTL) & (1UL << 29UL)) != 0UL) {
		ret = true;
	}

	return ret;
}

void vcpu_kick_splitlock_emulation(struct acrn_vcpu *cur_vcpu)
{
	struct acrn_vcpu *other;
	uint16_t i;

	if (cur_vcpu->vm->hw.created_vcpus > 1U) {
		get_split_lock(cur_vcpu->vm);

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

		put_split_lock(cur_vcpu->vm);
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
						/* Enable MTF to start single-stepping execution */
						vcpu->arch.proc_vm_exec_ctrls |= VMX_PROCBASED_CTLS_MON_TRAP;
						exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, vcpu->arch.proc_vm_exec_ctrls);
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
		default:
			break;
		}
	}

	return status;
}
