/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Author: Haicheng Li <haicheng.li@intel.com>
 *         Yifan Liu <yifan1.liu@intel.com>
 *
 * Re-written from OpenSBI sbi_ecall.c
 * The original code is licensed under BSD-2-Clause
 */

#include <vcpu.h>

#include <vcpu.h>
#include <vm.h>
#include <logmsg.h>

#include <asm/sbi.h>
#include <asm/guest/vsbi.h>

const struct acrn_vsbi_extension *vcpu_find_extension(struct acrn_vcpu *vcpu, uint64_t eid)
{
	const struct acrn_vsbi_extension *e, *ret = NULL;
	struct acrn_vm *vm = vcpu->vm;
	int i;

	for (i = 0; i < vm->arch_vm.n_vsbi_exts; i++) {
		e = vm->arch_vm.vsbi_exts[i];
		if ((eid >= e->eid_start) && (eid <= e->eid_end)) {
			ret = e;
			break;
		}
	}

	return ret;
}

int32_t vsbi_exit_handler(struct acrn_vcpu *vcpu)
{
	struct cpu_regs *regs = &(vcpu->arch.regs);
	uint64_t args[6];
	uint64_t eid = regs->a7, fid = regs->a6;
	int32_t ret = SBI_ERR_NOT_SUPPORTED;
	struct vsbi_ret out = { 0 };
	const struct acrn_vsbi_extension *e;

	args[0] = regs->a0;
	args[1] = regs->a1;
	args[2] = regs->a2;
	args[3] = regs->a3;
	args[4] = regs->a4;
	args[5] = regs->a5;

	e = vcpu_find_extension(vcpu, eid);
	if (e && e->handler) {
		pr_dbg("vsbi: eid: 0x%x fid: 0x%x, args 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
				eid, fid, args[0], args[1], args[2], args[3], args[4], args[5]);
		ret = e->handler(vcpu, eid, fid, args, &out);
	} else {
		pr_err("Unsupported ecall request: eid 0x%x fid 0x%x", eid, fid);
	}

	if (!out.vcpu_retain_pc) {
		regs->epc += 4;
	}

	regs->a0 = ret;
	regs->a1 = out.value;

	return 0;
}

extern const struct acrn_vsbi_extension vsbi_ext_base;
extern const struct acrn_vsbi_extension vsbi_ext_hsm;
extern const struct acrn_vsbi_extension vsbi_ext_srst;
extern const struct acrn_vsbi_extension vsbi_ext_acrn;
extern const struct acrn_vsbi_extension vsbi_ext_dbcn;
static const struct acrn_vsbi_extension *vsbi_extensions[MAX_NUM_SUPPORTED_VSBI_EXT] = {
	&vsbi_ext_base,
	&vsbi_ext_hsm,
	&vsbi_ext_srst,
	&vsbi_ext_acrn,
	&vsbi_ext_dbcn,
};

void init_vsbi(struct acrn_vm *vm)
{
	uint16_t i;
	int32_t ret;
	const struct acrn_vsbi_extension *e;

	for (i = 0; (i < MAX_NUM_SUPPORTED_VSBI_EXT) && (vsbi_extensions[i] != NULL); i++) {
		e = vsbi_extensions[i];

		/* Unconditionally register extension if no probe method is provided.
		 * if probe method is provided, register only when probe success.
		 */
		if (e->probe == NULL) {
			vm->arch_vm.vsbi_exts[vm->arch_vm.n_vsbi_exts++] = e;
		} else {
			ret = e->probe(vm);
			if (ret == 0) {
				vm->arch_vm.vsbi_exts[vm->arch_vm.n_vsbi_exts++] = e;
			} else if (ret != -ENOSYS) {
				pr_err("Failed to register %s to VM%d, ret: %d", e->name, vm->vm_id, ret);
			} else {
				/* hide extension from this vm */
			}
		}
	}
}
