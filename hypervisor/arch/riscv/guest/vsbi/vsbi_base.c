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
	const struct acrn_vsbi_extension *e;

	switch (func_id) {
	case SBI_BASE_FID_GET_SPEC_VERSION:
		out->value = VSBI_SPEC_VERSION_MAJOR << 24 | VSBI_SPEC_VERSION_MINOR;
		break;
	case SBI_BASE_FID_GET_IMP_ID:
		out->value = VSBI_ACRN_IMPID;
		break;
	case SBI_BASE_FID_GET_IMP_VERSION:
		out->value = VSBI_ACRN_VERSION_MAJOR << 24 | VSBI_ACRN_VERSION_MINOR;
		break;
	case SBI_BASE_FID_GET_MVENDORID:
		out->value = vcpu->vm->arch_vm.mvendorid;;
		break;
	case SBI_BASE_FID_GET_MARCHID:
		out->value = vcpu->vm->arch_vm.marchid;
		break;
	case SBI_BASE_FID_GET_MIMPID:
		out->value = vcpu->vm->arch_vm.mimpid;
		break;
	case SBI_BASE_FID_PROBE_EXT:
		e = vcpu_find_extension(vcpu, args[0]);
		out->value = (e == NULL) ? 0 : 1;
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
		break;
	}

	return ret;
}

const struct acrn_vsbi_extension vsbi_ext_base = {
	.name = "base",
	.eid_start = SBI_EID_BASE,
	.eid_end = SBI_EID_BASE,
	.handler = vcpu_sbi_base_ecall_handler,
};
