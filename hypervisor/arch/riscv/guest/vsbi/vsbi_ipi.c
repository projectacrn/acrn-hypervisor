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
#include <asm/trap.h>
#include <asm/guest/vsbi.h>
#include <asm/guest/virq.h>
#include <logmsg.h>

static int32_t vcpu_sbi_ipi_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
	uint64_t func_id, uint64_t *args, __unused struct vsbi_ret *out)
{
	int32_t ret = SBI_SUCCESS;
	uint16_t i;
	struct acrn_vcpu *tmp_vcpu;
	uint64_t mask = args[0];
	uint64_t mask_base = args[1];
	uint64_t bit_index = 0;
	int early_exit = 0;
	int is_broadcast = (mask_base == UINT64_MAX);

	switch (func_id) {
	case SBI_IPI_FID_SEND_IPI:
		foreach_vcpu(i, vcpu->vm, tmp_vcpu) {
			if (!is_broadcast) {
				if (tmp_vcpu->vcpu_id < mask_base)
					continue;
				bit_index = tmp_vcpu->vcpu_id - mask_base;
				if (bit_index >= 64UL) {
					early_exit = 1;
					break;
				}
				if (!(mask & (1UL << bit_index)))
					continue;
			}

			/* asserts a VS-level software interrupt to target VCPU */
			ret = vcpu_set_intr(tmp_vcpu, TRAP_CAUSE_IRQ_VS_SOFT);
			if (ret < 0) {
				pr_err("vsbi ipi: failed to send ipi to vcpu %hu", tmp_vcpu->vcpu_id);
				early_exit = 1;
				break;
			}
		}

		if (!is_broadcast && early_exit)
			ret = SBI_ERR_INVALID_PARAM;
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_ipi = {
	.name = "ipi",
	.eid_start = SBI_EID_IPI,
	.eid_end = SBI_EID_IPI,
	.handler = vcpu_sbi_ipi_ecall_handler,
};
