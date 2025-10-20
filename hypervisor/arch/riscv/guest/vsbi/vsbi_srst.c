/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>
#include <vm.h>
#include <per_cpu.h>

#include <asm/sbi.h>
#include <asm/guest/vsbi.h>

static const char *vcpu_sbi_srst_reason_to_string(uint64_t reason)
{
	const char *r = "unknown";
	if ((reason >> 32) == 0) {
		if (reason == SBI_SRST_RESET_REASON_NONE) {
			r = "none";
		} else if (reason == SBI_SRST_RESET_REASON_SYSFAIL) {
			r = "system failure";
		} else {
			/* TODO: Add SBI specific/Vendor platform specific later */
		}
	}
	return r;
}

static int32_t vcpu_sbi_srst_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
			       uint64_t func_id, uint64_t *args, __unused struct vsbi_ret *out)
{
	int32_t ret = SBI_ERR_NOT_SUPPORTED;
	char *shutdown_type = "unknown";
	struct acrn_vm *vm = vcpu->vm;

	if (func_id == SBI_EXT_SRST_RESET) {
		switch (args[0]) {
			case SBI_SRST_RESET_TYPE_SHUTDOWN:
				/* pause vm and destroy */
				shutdown_type = "shutdown";
				get_vm_lock(vm);
				pause_vm(vm);
				put_vm_lock(vm);
				bitmap_set_non_atomic(vm->vm_id,
						&per_cpu(shutdown_vm_bitmap, pcpuid_from_vcpu(vcpu)));
				make_shutdown_vm_request(pcpuid_from_vcpu(vcpu));
				ret = SBI_SUCCESS;
				break;
			case SBI_SRST_RESET_TYPE_COLD_REBOOT:
			case SBI_SRST_RESET_TYPE_WARM_REBOOT:
				shutdown_type = SBI_SRST_RESET_TYPE_COLD_REBOOT ? \
								"cold reboot" : "warm reboot";

				/* TODO: to be implemented */
				ret = SBI_SUCCESS;
				break;
			default:
				break;
		}
		pr_info("VM%d %s, reason: %s", vcpu->vm->vm_id, shutdown_type,
				vcpu_sbi_srst_reason_to_string(args[1]));
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_srst = {
	.name = "srst",
	.eid_start = SBI_EID_SRST,
	.eid_end = SBI_EID_SRST,
	.handler = vcpu_sbi_srst_ecall_handler,
};
