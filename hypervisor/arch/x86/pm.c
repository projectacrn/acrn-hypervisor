/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <hypervisor.h>

void restore_msrs(void)
{
#ifdef STACK_PROTECTOR
	struct stack_canary *psc = &get_cpu_var(stack_canary);

	msr_write(MSR_IA32_FS_BASE, (uint64_t)psc);
#endif
}
