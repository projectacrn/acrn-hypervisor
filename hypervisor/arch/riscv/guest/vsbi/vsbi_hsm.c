/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Haicheng Li <haicheng.li@intel.com>
 *         Yifan Liu <yifan1.liu@intel.com>
 */

#include <types.h>
#include <vcpu.h>
#include <vm.h>

#include <asm/sbi.h>
#include <asm/guest/vsbi.h>

static int32_t vcpu_sbi_hsm_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
			       uint64_t func_id, uint64_t *args, struct vsbi_ret *out)
{
	int32_t ret = SBI_SUCCESS;
	struct acrn_vcpu *tvcpu;
	struct acrn_vm *vm = vcpu->vm;
	uint32_t tvhartid;

	tvhartid = (uint32_t)args[0];

	switch (func_id) {
		case SBI_HSM_FID_HART_START:
			tvcpu = vcpu_from_vhartid(vm, tvhartid);
			if (tvcpu == NULL) {
				ret = SBI_ERR_INVALID_PARAM;
			} else if (tvcpu->state == VCPU_RUNNING) {
				ret = SBI_ERR_ALREADY_AVAILABLE;
			} else if (tvcpu->state != VCPU_INIT) {
				ret = SBI_ERR_FAILED;
				/* TODO: One more else if to check if the address (args[1]) is valid */
			} else {
				tvcpu->arch.regs.epc = args[1];
				tvcpu->arch.regs.a0 = tvhartid;
				tvcpu->arch.regs.a1 = args[2];
				launch_vcpu(tvcpu);
			}
			break;
		case SBI_HSM_FID_HART_STOP:
			/*
			 * Should not return to guest
			 * when successfully stopped
			 */
			get_vm_lock(vm);
			zombie_vcpu(vcpu);
			put_vm_lock(vm);
			break;
		case SBI_HSM_FID_HART_GET_STATUS:
			/* TODO: PENDING_ state implementation */
			tvcpu = vcpu_from_vhartid(vm, tvhartid);
			if (tvcpu != NULL) {
				out->value = (tvcpu->state == VCPU_RUNNING) ? SBI_HSM_STATE_STARTED :
					SBI_HSM_STATE_STOPPED;
			} else {
				ret = SBI_ERR_INVALID_PARAM;
			}
			break;
		case SBI_HSM_FID_HART_SUSPEND:
			/* TODO: To be implemented */
			pr_err("Unimplemented hart suspend request");
			ret = SBI_ERR_NOT_SUPPORTED;
			break;
		default:
			ret = SBI_ERR_NOT_SUPPORTED;
			break;
	}
	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_hsm = {
	.name = "hsm",
	.eid_start = SBI_EID_HSM,
	.eid_end = SBI_EID_HSM,
	.handler = vcpu_sbi_hsm_ecall_handler,
};
