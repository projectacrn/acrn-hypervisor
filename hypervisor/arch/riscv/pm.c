/*
 * Copyright (C) 2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <host_pm.h>
#include <asm/sbi.h>

static const char *reset_mode_str[] = {
	"shutdown",
	"cold reboot",
	"warm reboot",
};

static void sbi_system_reset(uint32_t rst_type, uint32_t rst_reason)
{
	sbiret ret;

	ret = sbi_ecall(rst_type, rst_reason, 0, 0, 0, 0, SBI_EXT_SRST_RESET, SBI_EID_SRST);

	/* above sbi call will not return if succeeded */
	panic("Failed to %s system, ret 0x%x", reset_mode_str[rst_type], ret.error);
}

void arch_shutdown_host()
{
	sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, 0U);
}

void arch_reset_host(bool warm)
{
	sbi_system_reset(warm ? SBI_SRST_RESET_TYPE_WARM_REBOOT \
			: SBI_SRST_RESET_TYPE_COLD_REBOOT, 0U);
}
