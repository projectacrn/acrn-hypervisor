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

static int32_t vcpu_sbi_acrn_ecall_handler(struct acrn_vcpu *vcpu, __unused uint64_t ext_id,
			       uint64_t func_id, uint64_t *args, struct vsbi_ret *out)
{
	(void)vcpu;
	(void)func_id;
	(void)args;
	(void)out;

	/* TODO: Hypercall implementation goes here */

	return 0;
}

static int32_t vsbi_acrn_probe(struct acrn_vm *vm)
{
	return is_service_vm(vm) ? 0 : -ENOSYS;
}

const struct acrn_vsbi_extension vsbi_ext_acrn = {
	.name = "acrn",
	.eid_start = SBI_EID_ACRN,
	.eid_end = SBI_EID_ACRN,
	.handler = vcpu_sbi_acrn_ecall_handler,
	.probe = vsbi_acrn_probe,
};
