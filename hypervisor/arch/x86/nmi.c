/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch/x86/irq.h>
#include <arch/x86/vmx.h>

void handle_nmi(__unused struct intr_excp_ctx *ctx)
{
	uint32_t value32;

	/*
	 * There is a window where we may miss the current request in this
	 * notification period when the work flow is as the following:
	 *
	 *       CPUx +                   + CPUr
	 *            |                   |
	 *            |                   +--+
	 *            |                   |  | Handle pending req
	 *            |                   <--+
	 *            +--+                |
	 *            |  | Set req flag   |
	 *            <--+                |
	 *            +------------------>---+
	 *            |     Send NMI      |  | Handle NMI
	 *            |                   <--+
	 *            |                   |
	 *            |                   |
	 *            |                   +--> vCPU enter
	 *            |                   |
	 *            +                   +
	 *
	 * So, here we enable the NMI-window exiting to trigger the next vmexit
	 * once there is no "virtual-NMI blocking" after vCPU enter into VMX non-root
	 * mode. Then we can process the pending request on time.
	 */
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
	value32 |= VMX_PROCBASED_CTLS_NMI_WINEXIT;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);
}
