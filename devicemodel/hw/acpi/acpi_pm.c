/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "vmm.h"
#include "vmmapi.h"
#include "dm.h"
#include "acpi.h"

uint8_t get_vcpu_px_cnt(struct vmctx *ctx, int vcpu_id)
{
	uint64_t pm_ioctl_buf = 0;
	enum pm_cmd_type cmd_type = PMCMD_GET_PX_CNT;

	pm_ioctl_buf = ((ctx->vmid << PMCMD_VMID_SHIFT) & PMCMD_VMID_MASK)
			| ((vcpu_id << PMCMD_VCPUID_SHIFT) & PMCMD_VCPUID_MASK)
			| cmd_type;

	if (vm_get_cpu_state(ctx, &pm_ioctl_buf)) {
		return 0;
	}

	return (uint8_t)pm_ioctl_buf;
}

int get_vcpu_px_data(struct vmctx *ctx, int vcpu_id,
			int px_num, struct cpu_px_data *vcpu_px_data)
{
	uint64_t *pm_ioctl_buf;
	enum pm_cmd_type cmd_type = PMCMD_GET_PX_DATA;

	pm_ioctl_buf = malloc(sizeof(struct cpu_px_data));
	if (!pm_ioctl_buf) {
		return -1;
	}

	*pm_ioctl_buf = ((ctx->vmid << PMCMD_VMID_SHIFT) & PMCMD_VMID_MASK)
		| ((vcpu_id << PMCMD_VCPUID_SHIFT) & PMCMD_VCPUID_MASK)
		| ((px_num << PMCMD_STATE_NUM_SHIFT) & PMCMD_STATE_NUM_MASK)
		| cmd_type;

	/* get and validate px data */
	if (vm_get_cpu_state(ctx, pm_ioctl_buf)) {
		free(pm_ioctl_buf);
		return -1;
	}

	memcpy(vcpu_px_data, pm_ioctl_buf,
			sizeof(struct cpu_px_data));

	free(pm_ioctl_buf);
	return 0;
}
