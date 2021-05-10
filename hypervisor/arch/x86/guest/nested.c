/*
 * Copyright (C) 2021 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <logmsg.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vm.h>
#include <asm/guest/nested.h>

/* The only purpose of this array is to serve the is_vmx_msr() function */
static const uint32_t vmx_msrs[NUM_VMX_MSRS] = {
	LIST_OF_VMX_MSRS
};

bool is_vmx_msr(uint32_t msr)
{
	bool found = false;
	uint32_t i;

	for (i = 0U; i < NUM_VMX_MSRS; i++) {
		if (msr == vmx_msrs[i]) {
			found = true;
			break;
		}
	}

	return found;
}

/*
 * @pre vcpu != NULL
 */
void init_vmx_msrs(__unused struct acrn_vcpu *vcpu)
{
	/* implemented in next patch */
}

/*
 * @pre vcpu != NULL
 */
int32_t read_vmx_msr(struct acrn_vcpu *vcpu, __unused uint32_t msr, uint64_t *val)
{
	uint64_t v = 0UL;
	int32_t err = 0;

	if (is_nvmx_configured(vcpu->vm)) {
		/* implemented in next patch */
	} else {
		err = -EACCES;
	}

	*val = v;
	return err;
}
