/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include <console.h>
#include <sprintf.h>
#include <logmsg.h>
#include <version.h>
#include <cpu.h>
#include "debug/shell_priv.h"


#define SHELL_RISCV_DEMO_SHELL		"riscv_demo_shell"
#define SHELL_RISCV_DEMO_SHELL_PARAM	NULL
#define SHELL_RISCV_DEMO_SHELL_HELP	"RISC-V shell demo"


static int32_t shell_riscv_demo_shell(__unused int32_t argc, __unused char **argv);

struct shell_cmd arch_shell_cmds[] = {
	{
		.str		= SHELL_RISCV_DEMO_SHELL,
		.cmd_param	= SHELL_RISCV_DEMO_SHELL_PARAM,
		.help_str	= SHELL_RISCV_DEMO_SHELL_HELP,
		.fcn		= shell_riscv_demo_shell,
	},
};

uint32_t arch_shell_cmds_sz = ARRAY_SIZE(arch_shell_cmds);

static int32_t shell_riscv_demo_shell(__unused int32_t argc, __unused char **argv)
{
	shell_puts("\r\n RISC-V SHELL DEMO\r\n");

	return 0;
}
