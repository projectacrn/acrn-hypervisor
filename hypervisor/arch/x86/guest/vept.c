/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/mmu.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/vmexit.h>
#include <asm/guest/ept.h>
#include <asm/guest/nested.h>

/**
 * @pre vcpu != NULL
 */
int32_t invept_vmexit_handler(struct acrn_vcpu *vcpu)
{
	struct invept_desc operand_gla_ept;
	uint64_t type;

	if (check_vmx_permission(vcpu)) {
		type = get_invvpid_ept_operands(vcpu, (void *)&operand_gla_ept, sizeof(operand_gla_ept));

		if (type > INVEPT_TYPE_ALL_CONTEXTS) {
			nested_vmx_result(VMfailValid, VMXERR_INVEPT_INVVPID_INVALID_OPERAND);
		} else {
			operand_gla_ept.eptp = gpa2hpa(vcpu->vm, operand_gla_ept.eptp);
			asm_invept(type, operand_gla_ept);
			nested_vmx_result(VMsucceed, 0);
		}
	}

	return 0;
}
