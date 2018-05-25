/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

int interrupt_init(uint32_t cpu_id)
{
	struct host_idt_descriptor *idtd = &HOST_IDTR;
	int status;

	set_idt(idtd);

	status = init_lapic(cpu_id);
	ASSERT(status == 0, "lapic init failed");
	if (status != 0)
		return -ENODEV;

	status = init_default_irqs(cpu_id);
	ASSERT(status == 0, "irqs init failed");
	if (status != 0)
		return -ENODEV;

#ifndef CONFIG_EFI_STUB
	CPU_IRQ_ENABLE();
#endif

	return status;
}
