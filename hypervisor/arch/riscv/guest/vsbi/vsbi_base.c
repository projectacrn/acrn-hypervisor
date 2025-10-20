/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <vcpu.h>
#include <vm.h>

#include <asm/sbi.h>
#include <asm/guest/vsbi.h>

static int32_t vcpu_sbi_base_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
			       uint64_t func_id, uint64_t *args, struct vsbi_ret *out)
{
	int ret = SBI_SUCCESS;
	sbiret r;
	const struct acrn_vsbi_extension *e;

	switch (func_id) {
	case SBI_BASE_FID_GET_SPEC_VERSION:
	case SBI_BASE_FID_GET_IMP_VERSION:
	case SBI_BASE_FID_GET_IMP_ID:
	case SBI_BASE_FID_GET_MVENDORID:
	case SBI_BASE_FID_GET_MARCHID:
	case SBI_BASE_FID_GET_MIMPID:
		/* TODO: Dummy. For now just passthrough everything */
		r = sbi_ecall(0, 0, 0, 0, 0, 0, func_id, SBI_EID_BASE);
		ret = r.error;
		out->value = r.value;
		break;
	case SBI_BASE_FID_PROBE_EXT:
		e = vcpu_find_extension(vcpu, args[0]);
		out->value = (e == NULL) ? 0 : 1;
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_base = {
	.name = "base",
	.eid_start = SBI_EID_BASE,
	.eid_end = SBI_EID_BASE,
	.handler = vcpu_sbi_base_ecall_handler,
};
