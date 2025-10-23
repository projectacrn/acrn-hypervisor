/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>
#include <vm.h>
#include <per_cpu.h>

#include <asm/sbi.h>
#include <asm/guest/vsbi.h>

/**
 * This function processes SBI timer-related ecalls from a virtual CPU. It handles
 * timer programming requests by setting the virtual stimecmp CSR.
 */
static int32_t vcpu_sbi_timer_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
			       uint64_t func_id, uint64_t *args, __unused struct vsbi_ret *out)
{
	int32_t ret;

	if (func_id == SBI_TIMER_FID_SET_TIMER) {
		vcpu->arch.gctx.vstimecmp = args[0];
		cpu_csr_write(CSR_VSTIMECMP, args[0]);
		ret = SBI_SUCCESS;
	} else {
		ret = SBI_ERR_NOT_SUPPORTED;
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_timer = {
	.name = "timer",
	.eid_start = SBI_EID_TIMER,
	.eid_end = SBI_EID_TIMER,
	.handler = vcpu_sbi_timer_ecall_handler,
};
