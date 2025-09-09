/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm/irq.h>
#include <asm/vmx.h>

#include <asm/guest/vcpu.h>
#include <asm/guest/virq.h>
#include <cpu.h>

void handle_nmi(__unused struct intr_excp_ctx *ctx)
{
	uint16_t pcpu_id = get_pcpu_id();
	struct acrn_vcpu *vcpu = get_running_vcpu(pcpu_id);

	/*
	 * If NMI occurs, inject it into current vcpu. Now just PMI is verified.
	 * For other kind of NMI, it may need to be checked further.
	 */
	vcpu_make_request(vcpu, ACRN_REQUEST_NMI);
}
