/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHELL_PRIV_H
#define SHELL_PRIV_H

#include <spinlock.h>

#define SHELL_CMD_MAX_LEN		100U
#define SHELL_STRING_MAX_LEN		(PAGE_SIZE << 2U)

/* Shell Command Function */
typedef int32_t (*shell_cmd_fn_t)(int32_t argc, char **argv);

/* Shell Command */
struct shell_cmd {
	char *str;		/* Command string */
	char *cmd_param;	/* Command parameter string */
	char *help_str;		/* Help text associated with the command */
	shell_cmd_fn_t fcn;	/* Command call-back function */

};

/* Shell Control Block */
struct shell {
	char input_line[2][SHELL_CMD_MAX_LEN + 1U];	/* current & last */
	uint32_t input_line_len;	/* Length of current input line */
	uint32_t input_line_active;	/* Active input line index */
	struct shell_cmd *cmds;	/* cmds supported */
	uint32_t cmd_count;		/* Count of cmds supported */
};

/* Shell Command list with parameters and help description */
#define SHELL_CMD_HELP			"help"
#define SHELL_CMD_HELP_PARAM		NULL
#define SHELL_CMD_HELP_HELP		"Display info about the supported shell commands."

#define SHELL_CMD_VM_LIST		"vm_list"
#define SHELL_CMD_VM_LIST_PARAM		NULL
#define SHELL_CMD_VM_LIST_HELP		"Lists all VMs (VM Name, VM ID, VM State)"

#define SHELL_CMD_VCPU_LIST		"vcpu_list"
#define SHELL_CMD_VCPU_LIST_PARAM	NULL
#define SHELL_CMD_VCPU_LIST_HELP	"Lists all VCPU in all VMs"

#define SHELL_CMD_VCPU_DUMPREG		"vcpu_dumpreg"
#define SHELL_CMD_VCPU_DUMPREG_PARAM	"<vm id, vcpu id>"
#define SHELL_CMD_VCPU_DUMPREG_HELP	"Dump registers for a specific vcpu"

#define SHELL_CMD_DUMPMEM		"dumpmem"
#define SHELL_CMD_DUMPMEM_PARAM		"<addr, length>"
#define SHELL_CMD_DUMPMEM_HELP		"Dump physical memory"

#define SHELL_CMD_SOS_CONSOLE		"sos_console"
#define SHELL_CMD_SOS_CONSOLE_PARAM	NULL
#define SHELL_CMD_SOS_CONSOLE_HELP	"Switch to SOS's console"

#define SHELL_CMD_INTERRUPT		"int"
#define SHELL_CMD_INTERRUPT_PARAM	NULL
#define SHELL_CMD_INTERRUPT_HELP	"show interrupt info per CPU"

#define SHELL_CMD_PTDEV			"pt"
#define SHELL_CMD_PTDEV_PARAM		NULL
#define SHELL_CMD_PTDEV_HELP		"show pass-through device info"

#define SHELL_CMD_REBOOT		"reboot"
#define SHELL_CMD_REBOOT_PARAM		NULL
#define SHELL_CMD_REBOOT_HELP		"trigger system reboot"

#define SHELL_CMD_IOAPIC		"dump_ioapic"
#define SHELL_CMD_IOAPIC_PARAM		NULL
#define SHELL_CMD_IOAPIC_HELP		"show native ioapic info"

#define SHELL_CMD_VIOAPIC		"vioapic"
#define SHELL_CMD_VIOAPIC_PARAM		"<vm id>"
#define SHELL_CMD_VIOAPIC_HELP		"show vioapic info"

#define SHELL_CMD_LOG_LVL		"loglevel"
#define SHELL_CMD_LOG_LVL_PARAM		"[<console_loglevel> [<mem_loglevel> " \
					"[npk_loglevel]]]"
#define SHELL_CMD_LOG_LVL_HELP		"get(para is NULL), or set loglevel [0-6]"

#define SHELL_CMD_CPUID			"cpuid"
#define SHELL_CMD_CPUID_PARAM		"<leaf> [subleaf]"
#define SHELL_CMD_CPUID_HELP		"cpuid leaf [subleaf], in hexadecimal"
#endif /* SHELL_PRIV_H */
