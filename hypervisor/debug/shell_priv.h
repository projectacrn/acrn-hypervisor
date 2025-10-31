/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHELL_PRIV_H
#define SHELL_PRIV_H

#include <board_info.h>
#include <spinlock.h>
#include <asm/page.h>

#define SHELL_CMD_MAX_LEN		100U
#define SHELL_STRING_MAX_LEN		(PAGE_SIZE << 2U)

#define TEMP_STR_SIZE		60U
#define MAX_STR_SIZE		256U
#define SHELL_LOG_BUF_SIZE		(PAGE_SIZE * MAX_PCPU_NUM / 2U)

extern int16_t console_vmid;
extern char shell_log_buf[SHELL_LOG_BUF_SIZE];

/* Shell Command Function */
typedef int32_t (*shell_cmd_fn_t)(int32_t argc, char **argv);

/* Shell Command */
struct shell_cmd {
	char *str;		/* Command string */
	char *cmd_param;	/* Command parameter string */
	char *help_str;		/* Help text associated with the command */
	shell_cmd_fn_t fcn;	/* Command call-back function */

};

#define MAX_BUFFERED_CMDS 8

/* Shell Control Block */
struct shell {
	/* a ring buffer to buffer former commands and use one as current active input */
	char buffered_line[MAX_BUFFERED_CMDS][SHELL_CMD_MAX_LEN + 1U];
	uint32_t input_line_len;	/* Length of current input line */
	int32_t input_line_active;	/* Active input line index */

	int32_t to_select_index; /* used for up/down key to select former cmds */
	uint32_t cursor_offset; /* cursor offset position from left input line */

	struct shell_cmd *cmds;	/* cmds supported */
	uint32_t cmd_count;	/* Count of cmds supported */

	struct shell_cmd *arch_cmds;	/* arch cmds supported */
	uint32_t arch_cmd_count;	/* arch Count of cmds supported */
};

/* Shell Command list with parameters and help description */
#define SHELL_CMD_HELP			"help"
#define SHELL_CMD_HELP_PARAM		NULL
#define SHELL_CMD_HELP_HELP		"Display information about supported hypervisor shell commands"

#define SHELL_CMD_VERSION		"version"
#define SHELL_CMD_VERSION_PARAM		NULL
#define SHELL_CMD_VERSION_HELP		"Display the HV version information"

#define SHELL_CMD_LOG_LVL		"loglevel"
#define SHELL_CMD_LOG_LVL_PARAM		"[<console_loglevel> [<mem_loglevel> [npk_loglevel]]]"
#define SHELL_CMD_LOG_LVL_HELP		"No argument: get the level of logging for the console, memory and npk. Set "\
					"the level by giving (up to) 3 parameters between 0 and 6 (verbose)"

#define SHELL_CMD_DUMP_HOST_MEM		"dump_host_mem"
#define SHELL_CMD_DUMP_HOST_MEM_PARAM	"<addr, length>"
#define SHELL_CMD_DUMP_HOST_MEM_HELP	"Dump host memory, starting at a given address(Hex), and for a given length (Dec in bytes)"

#define SHELL_CMD_VM_LIST		"vm_list"
#define SHELL_CMD_VM_LIST_PARAM		NULL
#define SHELL_CMD_VM_LIST_HELP		"List all VMs, displaying the VM UUID, ID, name and state"

#define SHELL_CMD_VCPU_LIST		"vcpu_list"
#define SHELL_CMD_VCPU_LIST_PARAM	NULL
#define SHELL_CMD_VCPU_LIST_HELP	"List all vCPUs in all VMs"

void shell_puts(const char *string_ptr);

#endif /* SHELL_PRIV_H */
