/*
 * Copyright (C) 2018-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <asm/lib/bits.h>
#include "shell_priv.h"
#include <asm/irq.h>
#include <console.h>
#include <asm/per_cpu.h>
#include <asm/vmx.h>
#include <asm/cpuid.h>
#include <asm/ioapic.h>
#include <ptdev.h>
#include <asm/guest/vm.h>
#include <sprintf.h>
#include <logmsg.h>
#include <version.h>
#include <shell.h>
#include <asm/guest/vmcs.h>
#include <asm/host_pm.h>

#define TEMP_STR_SIZE		60U
#define MAX_STR_SIZE		256U
#define SHELL_PROMPT_STR	"ACRN:\\>"

#define SHELL_LOG_BUF_SIZE		(PAGE_SIZE * MAX_PCPU_NUM / 2U)
static char shell_log_buf[SHELL_LOG_BUF_SIZE];

/* Input Line Other - Switch to the "other" input line (there are only two
 * input lines total).
 */

static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv);
static int32_t shell_version(__unused int32_t argc, __unused char **argv);
static int32_t shell_list_vm(__unused int32_t argc, __unused char **argv);
static int32_t shell_list_vcpu(__unused int32_t argc, __unused char **argv);
static int32_t shell_vcpu_dumpreg(int32_t argc, char **argv);
static int32_t shell_dump_host_mem(int32_t argc, char **argv);
static int32_t shell_dump_guest_mem(int32_t argc, char **argv);
static int32_t shell_to_vm_console(int32_t argc, char **argv);
static int32_t shell_show_cpu_int(__unused int32_t argc, __unused char **argv);
static int32_t shell_show_ptdev_info(__unused int32_t argc, __unused char **argv);
static int32_t shell_show_vioapic_info(int32_t argc, char **argv);
static int32_t shell_show_ioapic_info(__unused int32_t argc, __unused char **argv);
static int32_t shell_loglevel(int32_t argc, char **argv);
static int32_t shell_cpuid(int32_t argc, char **argv);
static int32_t shell_reboot(int32_t argc, char **argv);
static int32_t shell_rdmsr(int32_t argc, char **argv);
static int32_t shell_wrmsr(int32_t argc, char **argv);

static struct shell_cmd shell_cmds[] = {
	{
		.str		= SHELL_CMD_HELP,
		.cmd_param	= SHELL_CMD_HELP_PARAM,
		.help_str	= SHELL_CMD_HELP_HELP,
		.fcn		= shell_cmd_help,
	},
	{
		.str		= SHELL_CMD_VERSION,
		.cmd_param	= SHELL_CMD_VERSION_PARAM,
		.help_str	= SHELL_CMD_VERSION_HELP,
		.fcn		= shell_version,
	},
	{
		.str		= SHELL_CMD_VM_LIST,
		.cmd_param	= SHELL_CMD_VM_LIST_PARAM,
		.help_str	= SHELL_CMD_VM_LIST_HELP,
		.fcn		= shell_list_vm,
	},
	{
		.str		= SHELL_CMD_VCPU_LIST,
		.cmd_param	= SHELL_CMD_VCPU_LIST_PARAM,
		.help_str	= SHELL_CMD_VCPU_LIST_HELP,
		.fcn		= shell_list_vcpu,
	},
	{
		.str		= SHELL_CMD_VCPU_DUMPREG,
		.cmd_param	= SHELL_CMD_VCPU_DUMPREG_PARAM,
		.help_str	= SHELL_CMD_VCPU_DUMPREG_HELP,
		.fcn		= shell_vcpu_dumpreg,
	},
	{
		.str		= SHELL_CMD_DUMP_HOST_MEM,
		.cmd_param	= SHELL_CMD_DUMP_HOST_MEM_PARAM,
		.help_str	= SHELL_CMD_DUMP_HOST_MEM_HELP,
		.fcn		= shell_dump_host_mem,
	},
	{
		.str		= SHELL_CMD_DUMP_GUEST_MEM,
		.cmd_param	= SHELL_CMD_DUMP_GUEST_MEM_PARAM,
		.help_str	= SHELL_CMD_DUMP_GUEST_MEM_HELP,
		.fcn		= shell_dump_guest_mem,
	},
	{
		.str		= SHELL_CMD_VM_CONSOLE,
		.cmd_param	= SHELL_CMD_VM_CONSOLE_PARAM,
		.help_str	= SHELL_CMD_VM_CONSOLE_HELP,
		.fcn		= shell_to_vm_console,
	},
	{
		.str		= SHELL_CMD_INTERRUPT,
		.cmd_param	= SHELL_CMD_INTERRUPT_PARAM,
		.help_str	= SHELL_CMD_INTERRUPT_HELP,
		.fcn		= shell_show_cpu_int,
	},
	{
		.str		= SHELL_CMD_PTDEV,
		.cmd_param	= SHELL_CMD_PTDEV_PARAM,
		.help_str	= SHELL_CMD_PTDEV_HELP,
		.fcn		= shell_show_ptdev_info,
	},
	{
		.str		= SHELL_CMD_VIOAPIC,
		.cmd_param	= SHELL_CMD_VIOAPIC_PARAM,
		.help_str	= SHELL_CMD_VIOAPIC_HELP,
		.fcn		= shell_show_vioapic_info,
	},
	{
		.str		= SHELL_CMD_IOAPIC,
		.cmd_param	= SHELL_CMD_IOAPIC_PARAM,
		.help_str	= SHELL_CMD_IOAPIC_HELP,
		.fcn		= shell_show_ioapic_info,
	},
	{
		.str		= SHELL_CMD_LOG_LVL,
		.cmd_param	= SHELL_CMD_LOG_LVL_PARAM,
		.help_str	= SHELL_CMD_LOG_LVL_HELP,
		.fcn		= shell_loglevel,
	},
	{
		.str		= SHELL_CMD_CPUID,
		.cmd_param	= SHELL_CMD_CPUID_PARAM,
		.help_str	= SHELL_CMD_CPUID_HELP,
		.fcn		= shell_cpuid,
	},
	{
		.str		= SHELL_CMD_REBOOT,
		.cmd_param	= SHELL_CMD_REBOOT_PARAM,
		.help_str	= SHELL_CMD_REBOOT_HELP,
		.fcn		= shell_reboot,
	},
	{
		.str		= SHELL_CMD_RDMSR,
		.cmd_param	= SHELL_CMD_RDMSR_PARAM,
		.help_str	= SHELL_CMD_RDMSR_HELP,
		.fcn		= shell_rdmsr,
	},
	{
		.str		= SHELL_CMD_WRMSR,
		.cmd_param	= SHELL_CMD_WRMSR_PARAM,
		.help_str	= SHELL_CMD_WRMSR_HELP,
		.fcn		= shell_wrmsr,
	},
};

/* for function key: up/down/right/left/home/end and delete key */
enum function_key {
	KEY_NONE,

	KEY_DELETE = 0x5B33,
	KEY_UP = 0x5B41,
	KEY_DOWN = 0x5B42,
	KEY_RIGHT = 0x5B43,
	KEY_LEFT = 0x5B44,
	KEY_END = 0x5B46,
	KEY_HOME = 0x5B48,
};

/* The initial log level*/
uint16_t console_loglevel = CONFIG_CONSOLE_LOGLEVEL_DEFAULT;
uint16_t mem_loglevel = CONFIG_MEM_LOGLEVEL_DEFAULT;
uint16_t npk_loglevel = CONFIG_NPK_LOGLEVEL_DEFAULT;

static struct shell hv_shell;
static struct shell *p_shell = &hv_shell;

static int32_t string_to_argv(char *argv_str, void *p_argv_mem,
		__unused uint32_t argv_mem_size,
		uint32_t *p_argc, char ***p_argv)
{
	uint32_t argc;
	char **argv;
	char *p_ch;

	/* Setup initial argument values. */
	argc = 0U;
	argv = NULL;

	/* Ensure there are arguments to be processed. */
	if (argv_str == NULL) {
		*p_argc = argc;
		*p_argv = argv;
		return -EINVAL;
	}

	/* Process the argument string (there is at least one element). */
	argv = (char **)p_argv_mem;
	p_ch = argv_str;

	/* Remove all spaces at the beginning of cmd*/
	while (*p_ch == ' ') {
		p_ch++;
	}

	while (*p_ch != 0) {
		/* Add argument (string) pointer to the vector. */
		argv[argc] = p_ch;

		/* Move past the vector entry argument string (in the
		 * argument string).
		 */
		while ((*p_ch != ' ') && (*p_ch != ',') && (*p_ch != 0)) {
			p_ch++;
		}

		/* Count the argument just processed. */
		argc++;

		/* Check for the end of the argument string. */
		if (*p_ch != 0) {
			/* Terminate the vector entry argument string
			 * and move to the next.
			 */
			*p_ch = 0;
			/* Remove all space in middile of cmdline */
			p_ch++;
			while (*p_ch == ' ') {
				p_ch++;
			}
		}
	}

	/* Update return parameters */
	*p_argc = argc;
	*p_argv = argv;

	return 0;
}

static struct shell_cmd *shell_find_cmd(const char *cmd_str)
{
	uint32_t i;
	struct shell_cmd *p_cmd = NULL;

	for (i = 0U; i < p_shell->cmd_count; i++) {
		p_cmd = &p_shell->cmds[i];
		if (strcmp(p_cmd->str, cmd_str) == 0) {
			return p_cmd;
		}
	}
	return NULL;
}

static char shell_getc(void)
{
	return console_getc();
}

static void shell_puts(const char *string_ptr)
{
	/* Output the string */
	(void)console_write(string_ptr, strnlen_s(string_ptr,
				SHELL_STRING_MAX_LEN));
}

static uint16_t sanitize_vmid(uint16_t vmid)
{
	uint16_t sanitized_vmid = vmid;
	char temp_str[TEMP_STR_SIZE];

	if (vmid >= CONFIG_MAX_VM_NUM) {
		snprintf(temp_str, TEMP_STR_SIZE,
			"VM ID given exceeds the MAX_VM_NUM(%u), using 0 instead\r\n",
			CONFIG_MAX_VM_NUM);
		shell_puts(temp_str);
		sanitized_vmid = 0U;
	}

	return sanitized_vmid;
}

static void clear_input_line(uint32_t len)
{
	while (len > 0) {
		len--;
		shell_puts("\b");
		shell_puts(" \b");
	}
}

static void set_cursor_pos(uint32_t left_offset)
{
	while (left_offset > 0) {
		left_offset--;
		shell_puts("\b");
	}
}

static void handle_delete_key(void)
{
	if (p_shell->cursor_offset < p_shell->input_line_len) {

		uint32_t delta = p_shell->input_line_len - p_shell->cursor_offset - 1;

		/* Send a space + backspace sequence to delete character */
		shell_puts(" \b");

		/* display the left input chars and remove former last one */
		shell_puts(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset + 1);
		shell_puts(" \b");

		set_cursor_pos(delta);

		memcpy(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset,
			p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset + 1, delta);

		/* Null terminate the last character to erase it */
		p_shell->buffered_line[p_shell->input_line_active][p_shell->input_line_len - 1] = 0;

		/* Reduce the length of the string by one */
		p_shell->input_line_len--;
	}
}

static void handle_updown_key(enum function_key key_value)
{
	int32_t to_select, current_select = p_shell->to_select_index;

	/* update current_select and p_shell->to_select_index as up/down key */
	if (key_value == KEY_UP) {
		/* if the ring buffer not full, just decrease one until to 0; if full, need handle overflow case */
		to_select = p_shell->to_select_index - 1;
		if (to_select < 0) {
			to_select += MAX_BUFFERED_CMDS;
		}

		if (p_shell->buffered_line[to_select][0] != '\0') {
			current_select = to_select;
		}

	} else {
		/* if down key and current is active line, not need update */
		if (p_shell->to_select_index != p_shell->input_line_active) {
			current_select = (p_shell->to_select_index + 1) % MAX_BUFFERED_CMDS;
		}
	}

	/* go up/down until first buffered cmd or current input line: user will know it is end to select */
	if (current_select != p_shell->input_line_active) {
		p_shell->to_select_index = current_select;
	}

	if (strcmp(p_shell->buffered_line[current_select], p_shell->buffered_line[p_shell->input_line_active]) != 0) {
		/* reset cursor pos and clear current input line first, then output selected cmd */
		if (p_shell->cursor_offset < p_shell->input_line_len) {
			shell_puts(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset);
		}

		clear_input_line(p_shell->input_line_len);
		shell_puts(p_shell->buffered_line[current_select]);

		size_t len = strnlen_s(p_shell->buffered_line[current_select], SHELL_CMD_MAX_LEN);

		memcpy_s(p_shell->buffered_line[p_shell->input_line_active], SHELL_CMD_MAX_LEN,
			p_shell->buffered_line[current_select], len + 1);
		p_shell->input_line_len = len;
		p_shell->cursor_offset = len;
	}
}

static void shell_handle_special_char(char ch)
{
	enum function_key key_value = KEY_NONE;

	switch (ch) {
	/* original function key value: ESC + key (2/3 bytes), so consume the next 2/3 characters */
	case 0x1b:
		key_value = (shell_getc() << 8) | shell_getc();
		if (key_value == KEY_DELETE) {
			(void)shell_getc(); /* delete key has one more byte */
		}

		switch (key_value) {
		case KEY_DELETE:
			handle_delete_key();
			break;
		case KEY_UP:
		case KEY_DOWN:
			handle_updown_key(key_value);
			break;
		case KEY_RIGHT:
			if (p_shell->cursor_offset < p_shell->input_line_len) {
				shell_puts(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset);
				p_shell->cursor_offset++;
				set_cursor_pos(p_shell->input_line_len - p_shell->cursor_offset);
			}
			break;
		case KEY_LEFT:
			if (p_shell->cursor_offset > 0) {
				p_shell->cursor_offset--;
				shell_puts("\b");
			}
			break;
		case KEY_END:
			if (p_shell->cursor_offset < p_shell->input_line_len) {
				shell_puts(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset);
				p_shell->cursor_offset = p_shell->input_line_len;
			}
			break;
		case KEY_HOME:
			if (p_shell->cursor_offset > 0) {
				set_cursor_pos(p_shell->cursor_offset);
				p_shell->cursor_offset = 0;
			}
			break;
		default:
			break;
		}

		break;
	default:
		/*
		 * Only the Escape character is treated as special character.
		 * All the other characters have been handled properly in
		 * shell_input_line, so they will not be handled in this API.
		 * Gracefully return if prior case clauses have not been met.
		 */
		break;
	}
}

static void handle_backspace_key(void)
{
	/* Ensure length is not 0 */
	if (p_shell->cursor_offset > 0U) {
		/* Echo backspace */
		shell_puts("\b");
		/* Send a space + backspace sequence to delete character */
		shell_puts(" \b");

		if (p_shell->cursor_offset < p_shell->input_line_len) {
			uint32_t delta = p_shell->input_line_len - p_shell->cursor_offset;

			/* display the left input-chars and remove the former last one */
			shell_puts(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset);
			shell_puts(" \b");

			set_cursor_pos(delta);
			memcpy(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset - 1,
				p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset, delta);
		}

		/* Null terminate the last character to erase it */
		p_shell->buffered_line[p_shell->input_line_active][p_shell->input_line_len - 1] = 0;

		/* Reduce the length of the string by one */
		p_shell->input_line_len--;
		p_shell->cursor_offset--;
	}
}

static void handle_input_char(char ch)
{
	uint32_t delta = p_shell->input_line_len - p_shell->cursor_offset;

	/* move the input from cursor offset back first */
	if (delta > 0) {
		memcpy_backwards(p_shell->buffered_line[p_shell->input_line_active] + p_shell->input_line_len,
			p_shell->buffered_line[p_shell->input_line_active] + p_shell->input_line_len - 1, delta);
	}

	p_shell->buffered_line[p_shell->input_line_active][p_shell->cursor_offset] = ch;

	/* Echo back the input */
	shell_puts(p_shell->buffered_line[p_shell->input_line_active] + p_shell->cursor_offset);
	set_cursor_pos(delta);

	/* Move to next character in string */
	p_shell->input_line_len++;
	p_shell->cursor_offset++;
}

static bool shell_input_line(void)
{
	bool done = false;
	char ch;

	ch = shell_getc();

	/* Check character */
	switch (ch) {
	/* Backspace */
	case '\b':
		handle_backspace_key();
		break;

	/* Carriage-return */
	case '\r':
		/* Echo carriage return / line feed */
		shell_puts("\r\n");

		/* Set flag showing line input done */
		done = true;

		/* Reset command length for next command processing */
		p_shell->input_line_len = 0U;
		p_shell->cursor_offset = 0U;
		break;

	/* Line feed */
	case '\n':
		/* Do nothing */
		break;

	/* All other characters */
	default:
		/* Ensure data doesn't exceed full terminal width */
		if (p_shell->input_line_len < SHELL_CMD_MAX_LEN) {
			/* See if a "standard" prINTable ASCII character received */
			if ((ch >= 32) && (ch <= 126)) {
				handle_input_char(ch);
			} else {
				/* call special character handler */
				shell_handle_special_char(ch);
			}
		} else {
			/* Echo carriage return / line feed */
			shell_puts("\r\n");

			/* Set flag showing line input done */
			done = true;

			/* Reset command length for next command processing */
			p_shell->input_line_len = 0U;
			p_shell->cursor_offset = 0U;
		}
		break;
	}


	return done;
}

static int32_t shell_process_cmd(const char *p_input_line)
{
	int32_t status = -EINVAL;
	struct shell_cmd *p_cmd;
	char cmd_argv_str[SHELL_CMD_MAX_LEN + 1U];
	int32_t cmd_argv_mem[sizeof(char *) * ((SHELL_CMD_MAX_LEN + 1U) >> 1U)];
	int32_t cmd_argc;
	char **cmd_argv;

	/* Copy the input line INTo an argument string to become part of the
	 * argument vector.
	 */
	(void)strncpy_s(&cmd_argv_str[0], SHELL_CMD_MAX_LEN + 1U, p_input_line, SHELL_CMD_MAX_LEN);
	cmd_argv_str[SHELL_CMD_MAX_LEN] = 0;

	/* Build the argv vector from the string. The first argument in the
	 * resulting vector will be the command string itself.
	 */

	/* NOTE: This process is destructive to the argument string! */

	(void) string_to_argv(&cmd_argv_str[0],
			(void *) &cmd_argv_mem[0],
			sizeof(cmd_argv_mem), (void *)&cmd_argc, &cmd_argv);

	/* Determine if there is a command to process. */
	if (cmd_argc != 0) {
		/* See if command is in cmds supported */
		p_cmd = shell_find_cmd(cmd_argv[0]);
		if (p_cmd == NULL) {
			shell_puts("\r\nError: Invalid command.\r\n");
			return -EINVAL;
		}

		status = p_cmd->fcn(cmd_argc, &cmd_argv[0]);
		if (status == -EINVAL) {
			shell_puts("\r\nError: Invalid parameters.\r\n");
		} else if (status != 0) {
			shell_puts("\r\nCommand launch failed.\r\n");
		} else {
			/* No other state currently, do nothing */
		}
	}

	return status;
}

static int32_t shell_process(void)
{
	int32_t status, former_index;
	char *p_input_line;

	/* Process current command (using active input line). */
	p_input_line = p_shell->buffered_line[p_shell->input_line_active];

	former_index = (p_shell->input_line_active + MAX_BUFFERED_CMDS - 1) % MAX_BUFFERED_CMDS;

	/* just buffer current cmd if current is not empty and not same with last buffered one */
	if ((strnlen_s(p_input_line, SHELL_CMD_MAX_LEN) > 0) &&
		(strcmp(p_input_line, p_shell->buffered_line[former_index]) != 0)) {
		p_shell->input_line_active = (p_shell->input_line_active + 1) % MAX_BUFFERED_CMDS;
	}

	p_shell->to_select_index = p_shell->input_line_active;

	/* Process command */
	status = shell_process_cmd(p_input_line);

	/* Now that the command is processed, zero fill the input buffer */
	(void)memset(p_shell->buffered_line[p_shell->input_line_active], 0, SHELL_CMD_MAX_LEN + 1U);

	/* Process command and return result to caller */
	return status;
}


void shell_kick(void)
{
	static bool is_cmd_cmplt = true;

	/* At any given instance, UART may be owned by the HV
	 * OR by the guest that has enabled the vUart.
	 * Show HV shell prompt ONLY when HV owns the
	 * serial port.
	 */
	/* Prompt the user for a selection. */
	if (is_cmd_cmplt) {
		shell_puts(SHELL_PROMPT_STR);
	}

	/* Get user's input */
	is_cmd_cmplt = shell_input_line();

	/* If user has pressed the ENTER then process
	 * the command
	 */
	if (is_cmd_cmplt) {
		/* Process current input line. */
		(void)shell_process();
	}
}


void shell_init(void)
{
	p_shell->cmds = shell_cmds;
	p_shell->cmd_count = ARRAY_SIZE(shell_cmds);

	p_shell->to_select_index = 0;

	/* Zero fill the input buffer */
	(void)memset(p_shell->buffered_line[p_shell->input_line_active], 0U, SHELL_CMD_MAX_LEN + 1U);
}

#define SHELL_ROWS	30
#define MAX_OUTPUT_LEN  80
static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv)
{
	struct shell_cmd *p_cmd = NULL;

	char str[MAX_STR_SIZE];
	char* help_str;
	/* Print title */
	shell_puts("\r\nRegistered Commands:\r\n\r\n");

	pr_dbg("shell: Number of registered commands = %u in %s\n",
		p_shell->cmd_count, __func__);

	/* Proceed based on the number of registered commands. */
	if (p_shell->cmd_count == 0U) {
		/* No registered commands */
		shell_puts("NONE\r\n");
	} else {
		int32_t i = 0;
		uint32_t j;

		for (j = 0U; j < p_shell->cmd_count; j++) {
			p_cmd = &p_shell->cmds[j];

			/* Check if we've filled the screen with info */
			/* i + 1 used to avoid 0%SHELL_ROWS=0 */
			if (((i + 1) % SHELL_ROWS) == 0) {
				/* Pause before we continue on to the next
				 * page.
				 */

				/* Print message to the user. */
				shell_puts("<*** Hit any key to continue ***>");

				/* Wait for a character from user (NOT USED) */
				(void)shell_getc();

				/* Print a new line after the key is hit. */
				shell_puts("\r\n");
			}

			i++;
			if (p_cmd->cmd_param == NULL)
				p_cmd->cmd_param = " ";
			(void)memset(str, ' ', sizeof(str));
			/* Output the command & parameter string */
			snprintf(str, MAX_OUTPUT_LEN, " %-15s%-64s",
					p_cmd->str, p_cmd->cmd_param);
			shell_puts(str);
			shell_puts("\r\n");

			help_str = p_cmd->help_str;
			while (strnlen_s(help_str, MAX_OUTPUT_LEN > 0)) {
				(void)memset(str, ' ', sizeof(str));
				if (strnlen_s(help_str, MAX_OUTPUT_LEN) > 65) {
					snprintf(str, MAX_OUTPUT_LEN, "               %-s", help_str);
					shell_puts(str);
					shell_puts("\r\n");
					help_str = help_str + 65;
				} else {
					snprintf(str, MAX_OUTPUT_LEN, "               %-s", help_str);
					shell_puts(str);
					shell_puts("\r\n");
					break;
				}
			}
		}
	}

	shell_puts("\r\n");

	return 0;
}

static int32_t shell_version(__unused int32_t argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];

	snprintf(temp_str, MAX_STR_SIZE, "HV: %s-%s-%s %s%s%s%s %s@%s build by %s %s\r\n",
		HV_BRANCH_VERSION, HV_COMMIT_TIME, HV_COMMIT_DIRTY, HV_BUILD_TYPE,
		(sizeof(HV_COMMIT_TAGS) > 1) ? "(tag: " : "", HV_COMMIT_TAGS, 
		(sizeof(HV_COMMIT_TAGS) > 1) ? ")" : "",
		HV_BUILD_SCENARIO, HV_BUILD_BOARD, HV_BUILD_USER, HV_BUILD_TIME);
	shell_puts(temp_str);

	return 0;
}

static int32_t shell_list_vm(__unused int32_t argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];
	struct acrn_vm *vm;
	struct acrn_vm_config *vm_config;
	uint16_t vm_id;
	char state[32];

	shell_puts("\r\nVM_ID VM_NAME                          VM_STATE"
		   "\r\n===== ================================ ========\r\n");

	for (vm_id = 0U; vm_id < CONFIG_MAX_VM_NUM; vm_id++) {
		vm = get_vm_from_vmid(vm_id);
		switch (vm->state) {
		case VM_CREATED:
			(void)strncpy_s(state, 32U, "Created", 32U);
			break;
		case VM_RUNNING:
			(void)strncpy_s(state, 32U, "Running", 32U);
			break;
		case VM_PAUSED:
			(void)strncpy_s(state, 32U, "Paused", 32U);
			break;
		case VM_POWERED_OFF:
			(void)strncpy_s(state, 32U, "Off", 32U);
			break;
		default:
			(void)strncpy_s(state, 32U, "Unknown", 32U);
			break;
		}
		vm_config = get_vm_config(vm_id);
		if (!is_poweroff_vm(vm)) {
			snprintf(temp_str, MAX_STR_SIZE, "  %-3d %-32s %-8s\r\n",
				vm_id, vm_config->name, state);

			/* Output information for this task */
			shell_puts(temp_str);
		}
	}

	return 0;
}

static int32_t shell_list_vcpu(__unused int32_t argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];
	struct acrn_vm *vm;
	struct acrn_vcpu *vcpu;
	char vcpu_state_str[32], thread_state_str[32];
	uint16_t i;
	uint16_t idx;

	shell_puts("\r\nVM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE    THREAD STATE"
		"\r\n=====    =======    =======    =========    ==========    ==========\r\n");

	for (idx = 0U; idx < CONFIG_MAX_VM_NUM; idx++) {
		vm = get_vm_from_vmid(idx);
		if (is_poweroff_vm(vm)) {
			continue;
		}
		foreach_vcpu(i, vm, vcpu) {
			switch (vcpu->state) {
			case VCPU_INIT:
				(void)strncpy_s(vcpu_state_str, 32U, "Init", 32U);
				break;
			case VCPU_RUNNING:
				(void)strncpy_s(vcpu_state_str, 32U, "Running", 32U);
				break;
			case VCPU_ZOMBIE:
				(void)strncpy_s(vcpu_state_str, 32U, "Zombie", 32U);
				break;
			default:
				(void)strncpy_s(vcpu_state_str, 32U, "Unknown", 32U);
				break;
			}

			switch (vcpu->thread_obj.status) {
			case THREAD_STS_RUNNING:
				(void)strncpy_s(thread_state_str, 32U, "RUNNING", 32U);
				break;
			case THREAD_STS_RUNNABLE:
				(void)strncpy_s(thread_state_str, 32U, "RUNNABLE", 32U);
				break;
			case THREAD_STS_BLOCKED:
				(void)strncpy_s(thread_state_str, 32U, "BLOCKED", 32U);
				break;
			default:
				(void)strncpy_s(thread_state_str, 32U, "UNKNOWN", 32U);
				break;
			}
			/* Create output string consisting of VM name
			 * and VM id
			 */
			snprintf(temp_str, MAX_STR_SIZE,
					"  %-9d %-10d %-7hu %-12s %-16s %-16s\r\n",
					vm->vm_id,
					pcpuid_from_vcpu(vcpu),
					vcpu->vcpu_id,
					is_vcpu_bsp(vcpu) ?
					"PRIMARY" : "SECONDARY",
					vcpu_state_str, thread_state_str);
			/* Output information for this task */
			shell_puts(temp_str);
		}
	}

	return 0;
}

#define DUMPREG_SP_SIZE	32
/* the input 'data' must != NULL and indicate a vcpu structure pointer */
static void dump_vcpu_reg(void *data)
{
	int32_t status;
	uint64_t i, fault_addr, tmp[DUMPREG_SP_SIZE];
	uint32_t err_code = 0;
	struct vcpu_dump *dump = data;
	struct acrn_vcpu *vcpu = dump->vcpu;
	char *str = dump->str;
	size_t len, size = dump->str_max;
	uint16_t pcpu_id = get_pcpu_id();
	struct acrn_vcpu *curr = get_running_vcpu(pcpu_id);

	/* switch vmcs */
	load_vmcs(vcpu);

	len = snprintf(str, size,
		"=  VM ID %d ==== CPU ID %hu========================\r\n"
		"=  RIP=0x%016lx  RSP=0x%016lx RFLAGS=0x%016lx\r\n"
		"=  CR0=0x%016lx  CR2=0x%016lx\r\n"
		"=  CR3=0x%016lx  CR4=0x%016lx\r\n"
		"=  RAX=0x%016lx  RBX=0x%016lx RCX=0x%016lx\r\n"
		"=  RDX=0x%016lx  RDI=0x%016lx RSI=0x%016lx\r\n"
		"=  RBP=0x%016lx  R8=0x%016lx R9=0x%016lx\r\n"
		"=  R10=0x%016lx  R11=0x%016lx R12=0x%016lx\r\n"
		"=  R13=0x%016lx  R14=0x%016lx  R15=0x%016lx\r\n",
		vcpu->vm->vm_id, vcpu->vcpu_id,
		vcpu_get_rip(vcpu),
		vcpu_get_gpreg(vcpu, CPU_REG_RSP),
		vcpu_get_rflags(vcpu),
		vcpu_get_cr0(vcpu), vcpu_get_cr2(vcpu),
		exec_vmread(VMX_GUEST_CR3), vcpu_get_cr4(vcpu),
		vcpu_get_gpreg(vcpu, CPU_REG_RAX),
		vcpu_get_gpreg(vcpu, CPU_REG_RBX),
		vcpu_get_gpreg(vcpu, CPU_REG_RCX),
		vcpu_get_gpreg(vcpu, CPU_REG_RDX),
		vcpu_get_gpreg(vcpu, CPU_REG_RDI),
		vcpu_get_gpreg(vcpu, CPU_REG_RSI),
		vcpu_get_gpreg(vcpu, CPU_REG_RBP),
		vcpu_get_gpreg(vcpu, CPU_REG_R8),
		vcpu_get_gpreg(vcpu, CPU_REG_R9),
		vcpu_get_gpreg(vcpu, CPU_REG_R10),
		vcpu_get_gpreg(vcpu, CPU_REG_R11),
		vcpu_get_gpreg(vcpu, CPU_REG_R12),
		vcpu_get_gpreg(vcpu, CPU_REG_R13),
		vcpu_get_gpreg(vcpu, CPU_REG_R14),
		vcpu_get_gpreg(vcpu, CPU_REG_R15));
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	/* dump sp */
	status = copy_from_gva(vcpu, tmp, vcpu_get_gpreg(vcpu, CPU_REG_RSP),
			DUMPREG_SP_SIZE*sizeof(uint64_t), &err_code,
			&fault_addr);
	if (status < 0) {
		/* copy_from_gva fail */
		len = snprintf(str, size, "Cannot handle user gva yet!\r\n");
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
	} else {
		len = snprintf(str, size, "\r\nDump RSP for vm %hu, from gva 0x%016lx\r\n",
			vcpu->vm->vm_id, vcpu_get_gpreg(vcpu, CPU_REG_RSP));
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;

		for (i = 0UL; i < 8UL; i++) {
			len = snprintf(str, size, "=  0x%016lx  0x%016lx 0x%016lx  0x%016lx\r\n",
					tmp[i*4UL], tmp[(i*4UL)+1UL], tmp[(i*4UL)+2UL], tmp[(i*4UL)+3UL]);
			if (len >= size) {
				goto overflow;
			}
			size -= len;
			str += len;
		}
	}
	if (curr != NULL) {
		load_vmcs(curr);
	}
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}

static int32_t shell_vcpu_dumpreg(int32_t argc, char **argv)
{
	int32_t status = 0;
	uint16_t vm_id;
	uint16_t vcpu_id, pcpu_id;
	struct acrn_vm *vm;
	struct acrn_vcpu *vcpu;
	uint64_t mask = 0UL;
	struct vcpu_dump dump;

	/* User input invalidation */
	if (argc != 3) {
		shell_puts("Please enter cmd with <vm_id, vcpu_id>\r\n");
		status = -EINVAL;
		goto out;
	}

	status = strtol_deci(argv[1]);
	if (status < 0) {
		goto out;
	}
	vm_id = sanitize_vmid((uint16_t)status);
	vcpu_id = (uint16_t)strtol_deci(argv[2]);

	vm = get_vm_from_vmid(vm_id);
	if (is_poweroff_vm(vm)) {
		shell_puts("No vm found in the input <vm_id, vcpu_id>\r\n");
		status = -EINVAL;
		goto out;
	}

	if (vcpu_id >= vm->hw.created_vcpus) {
		shell_puts("vcpu id is out of range\r\n");
		status = -EINVAL;
		goto out;
	}

	vcpu = vcpu_from_vid(vm, vcpu_id);
	if (vcpu->state == VCPU_OFFLINE) {
		shell_puts("vcpu is offline\r\n");
		status = -EINVAL;
		goto out;
	}

	pcpu_id = pcpuid_from_vcpu(vcpu);
	dump.vcpu = vcpu;
	dump.str = shell_log_buf;
	dump.str_max = SHELL_LOG_BUF_SIZE;
	bitmap_set_nolock(pcpu_id, &mask);
	smp_call_function(mask, dump_vcpu_reg, &dump);
	shell_puts(shell_log_buf);
	status = 0;

out:
	return status;
}

static int32_t shell_dump_host_mem(int32_t argc, char **argv)
{
	uint64_t *hva;
	int32_t ret;
	uint32_t i, length, loop_cnt;
	char temp_str[MAX_STR_SIZE];

	/* User input invalidation */
	if (argc != 3) {
		ret = -EINVAL;
	} else	{
		hva = (uint64_t *)strtoul_hex(argv[1]);
		length = (uint32_t)strtol_deci(argv[2]);

		snprintf(temp_str, MAX_STR_SIZE, "Dump physical memory addr: 0x%016lx, length %d:\r\n", hva, length);
		shell_puts(temp_str);
		/* Change the length to a multiple of 32 if the length is not */
		loop_cnt = ((length & 0x1fU) == 0U) ? ((length >> 5U)) : ((length >> 5U) + 1U);
		for (i = 0U; i < loop_cnt; i++) {
			snprintf(temp_str, MAX_STR_SIZE, "HVA(0x%llx): 0x%016lx  0x%016lx  0x%016lx  0x%016lx\r\n",
					hva, *hva, *(hva + 1UL), *(hva + 2UL), *(hva + 3UL));
			hva += 4UL;
			shell_puts(temp_str);
		}
		ret = 0;
	}

	return ret;
}

static void dump_guest_mem(void *data)
{
	uint64_t i, fault_addr;
	uint32_t err_code = 0;
	uint64_t loop_cnt;
	uint64_t buf[4];
	char temp_str[MAX_STR_SIZE];
	struct guest_mem_dump *dump = (struct guest_mem_dump *)data;
	uint64_t length = dump->len;
	uint64_t gva = dump->gva;
	struct acrn_vcpu *vcpu = dump->vcpu;
	uint16_t pcpu_id = get_pcpu_id();
	struct acrn_vcpu *curr = get_running_vcpu(pcpu_id);

	load_vmcs(vcpu);

	/* Change the length to a multiple of 32 if the length is not */
	loop_cnt = ((length & 0x1fUL) == 0UL) ? ((length >> 5UL)) : ((length >> 5UL) + 1UL);

	for (i = 0UL; i < loop_cnt; i++) {

		if (copy_from_gva(vcpu, buf, gva, 32U, &err_code, &fault_addr) != 0) {
			printf("copy_from_gva error! err_code=0x%x fault_addr=0x%llx\r\n", err_code, fault_addr);
			break;
		}
		snprintf(temp_str, MAX_STR_SIZE, "GVA(0x%llx):  0x%016lx  0x%016lx  0x%016lx  0x%016lx\r\n",
				gva, buf[0], buf[1], buf[2], buf[3]);
		shell_puts(temp_str);
		gva += 32UL;
	}
	if (curr != NULL) {
		load_vmcs(curr);
	}
}

static int32_t shell_dump_guest_mem(int32_t argc, char **argv)
{
	uint16_t vm_id, pcpu_id;
	int32_t ret;
	uint64_t gva;
	uint64_t length;
	uint64_t mask = 0UL;
	struct acrn_vm *vm;
	struct acrn_vcpu *vcpu = NULL;
	struct guest_mem_dump dump;

	/* User input invalidation */
	if (argc != 4) {
		ret = -EINVAL;
	} else {
		vm_id = sanitize_vmid((uint16_t)strtol_deci(argv[1]));
		gva = strtoul_hex(argv[2]);
		length = (uint64_t)strtol_deci(argv[3]);

		vm = get_vm_from_vmid(vm_id);
		vcpu = vcpu_from_vid(vm, BSP_CPU_ID);

		dump.vcpu = vcpu;
		dump.gva = gva;
		dump.len = length;

		pcpu_id = pcpuid_from_vcpu(vcpu);
		bitmap_set_nolock(pcpu_id, &mask);
		smp_call_function(mask, dump_guest_mem, &dump);
		ret = 0;
	}

	return ret;
}

static int32_t shell_to_vm_console(int32_t argc, char **argv)
{
	char temp_str[TEMP_STR_SIZE];
	uint16_t vm_id = 0U;

	struct acrn_vm *vm;
	struct acrn_vuart *vu;

	if (argc == 2) {
		vm_id = sanitize_vmid((uint16_t)strtol_deci(argv[1]));
	}

	/* Get the virtual device node */
	vm = get_vm_from_vmid(vm_id);
	if (is_poweroff_vm(vm)) {
		shell_puts("VM is not valid \n");
		return -EINVAL;
	}
	vu = vm_console_vuart(vm);
	if (!vu->active) {
		shell_puts("vuart console is not active \n");
		return 0;
	}
	console_vmid = vm_id;
	/* Output that switching to Service VM shell */
	snprintf(temp_str, TEMP_STR_SIZE, "\r\n----- Entering VM %d Shell -----\r\n", vm_id);

	shell_puts(temp_str);

	return 0;
}

/**
 * @brief Get the interrupt statistics
 *
 * It's for debug only.
 *
 * @param[in]	str_max	The max size of the string containing interrupt info
 * @param[inout]	str_arg	Pointer to the output interrupt info
 */
static void get_cpu_interrupt_info(char *str_arg, size_t str_max)
{
	char *str = str_arg;
	uint16_t pcpu_id;
	uint32_t irq, vector;
	size_t len, size = str_max;
	uint16_t pcpu_nums = get_pcpu_nums();

	len = snprintf(str, size, "\r\nIRQ\tVECTOR");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	for (pcpu_id = 0U; pcpu_id < pcpu_nums; pcpu_id++) {
		len = snprintf(str, size, "\tCPU%d", pcpu_id);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
	}

	for (irq = 0U; irq < NR_IRQS; irq++) {
		vector = irq_to_vector(irq);
		if (bitmap_test((uint16_t)(irq & 0x3FU),
			irq_alloc_bitmap + (irq >> 6U))
			&& (vector != VECTOR_INVALID)) {
			len = snprintf(str, size, "\r\n%d\t0x%X", irq, vector);
			if (len >= size) {
				goto overflow;
			}
			size -= len;
			str += len;

			for (pcpu_id = 0U; pcpu_id < pcpu_nums; pcpu_id++) {
				len = snprintf(str, size, "\t%d", per_cpu(irq_count, pcpu_id)[irq]);
				if (len >= size) {
					goto overflow;
				}
				size -= len;
				str += len;
			}
		}
	}
	snprintf(str, size, "\r\n");
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}

static int32_t shell_show_cpu_int(__unused int32_t argc, __unused char **argv)
{
	get_cpu_interrupt_info(shell_log_buf, SHELL_LOG_BUF_SIZE);
	shell_puts(shell_log_buf);
	return 0;
}

static void get_entry_info(const struct ptirq_remapping_info *entry, char *type,
		uint32_t *irq, uint32_t *vector, uint64_t *dest, bool *lvl_tm,
		uint32_t *pgsi, uint32_t *vgsi, union pci_bdf *bdf, union pci_bdf *vbdf)
{
	if (is_entry_active(entry)) {
		if (entry->intr_type == PTDEV_INTR_MSI) {
			(void)strncpy_s(type, 16U, "MSI", 16U);
			*dest = entry->pmsi.addr.bits.dest_field;
			if (entry->pmsi.data.bits.trigger_mode == MSI_DATA_TRGRMODE_LEVEL) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pgsi = INVALID_INTERRUPT_PIN;
			*vgsi = INVALID_INTERRUPT_PIN;
			bdf->value = entry->phys_sid.msi_id.bdf;
			vbdf->value = entry->virt_sid.msi_id.bdf;
		} else {
			uint32_t phys_irq = entry->allocated_pirq;
			union ioapic_rte rte;

			if (entry->virt_sid.intx_id.ctlr == INTX_CTLR_IOAPIC) {
				(void)strncpy_s(type, 16U, "IOAPIC", 16U);
			} else {
				(void)strncpy_s(type, 16U, "PIC", 16U);
			}
			ioapic_get_rte(phys_irq, &rte);
			*dest = rte.bits.dest_field;
			if (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pgsi = entry->phys_sid.intx_id.gsi;
			*vgsi = entry->virt_sid.intx_id.gsi;
			bdf->value = 0U;
			vbdf->value = 0U;
		}
		*irq = entry->allocated_pirq;
		*vector = irq_to_vector(entry->allocated_pirq);
	} else {
		(void)strncpy_s(type, 16U, "NONE", 16U);
		*irq = IRQ_INVALID;
		*vector = 0U;
		*dest = 0UL;
		*lvl_tm = 0;
		*pgsi = ~0U;
		*vgsi = ~0U;
		bdf->value = 0U;
		vbdf->value = 0U;
	}
}

static void get_ptdev_info(char *str_arg, size_t str_max)
{
	char *str = str_arg;
	struct ptirq_remapping_info *entry;
	uint16_t idx;
	size_t len, size = str_max;
	uint32_t irq, vector;
	char type[16];
	uint64_t dest;
	bool lvl_tm;
	uint32_t pgsi, vgsi;
	union pci_bdf bdf, vbdf;

	len = snprintf(str, size, "\r\nVM\tTYPE\tIRQ\tVEC\tDEST\tTM\tGSI\tVGSI\tBDF\tVBDF");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if (is_entry_active(entry)) {
			get_entry_info(entry, type, &irq, &vector, &dest, &lvl_tm, &pgsi, &vgsi,
					&bdf, &vbdf);
			len = snprintf(str, size, "\r\n%d\t%s\t%d\t0x%X\t0x%X",
					entry->vm->vm_id, type, irq, vector, dest);
			if (len >= size) {
				goto overflow;
			}
			size -= len;
			str += len;

			len = snprintf(str, size, "\t%s\t%hhu\t%hhu\t%x:%x.%x\t%x:%x.%x",
					is_entry_active(entry) ? (lvl_tm ? "level" : "edge") : "none",
					pgsi, vgsi, bdf.bits.b, bdf.bits.d, bdf.bits.f,
					vbdf.bits.b, vbdf.bits.d, vbdf.bits.f);
			if (len >= size) {
				goto overflow;
			}
			size -= len;
			str += len;
		}
	}

	snprintf(str, size, "\r\n");
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}

static int32_t shell_show_ptdev_info(__unused int32_t argc, __unused char **argv)
{
	get_ptdev_info(shell_log_buf, SHELL_LOG_BUF_SIZE);
	shell_puts(shell_log_buf);

	return 0;
}

static void get_vioapic_info(char *str_arg, size_t str_max, uint16_t vmid)
{
	char *str = str_arg;
	size_t len, size = str_max;
	union ioapic_rte rte;
	uint32_t delmode, vector, dest;
	bool level, phys, remote_irr, mask;
	struct acrn_vm *vm = get_vm_from_vmid(vmid);
	uint32_t gsi, gsi_count;

	if (is_poweroff_vm(vm)) {
		len = snprintf(str, size, "\r\nvm is not exist for vmid %hu", vmid);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
		goto END;
	}

	len = snprintf(str, size, "\r\nPIN\tVEC\tDM\tDEST\tTM\tDELM\tIRR\tMASK");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	gsi_count = get_vm_gsicount(vm);
	rte.full = 0UL;
	for (gsi = 0U; gsi < gsi_count; gsi++) {
		if (is_service_vm(vm) && (!is_gsi_valid(gsi))) {
			continue;
		}
		vioapic_get_rte(vm, gsi, &rte);
		mask = (rte.bits.intr_mask == IOAPIC_RTE_MASK_SET);
		remote_irr = (rte.bits.remote_irr == IOAPIC_RTE_REM_IRR);
		phys = (rte.bits.dest_mode == IOAPIC_RTE_DESTMODE_PHY);
		delmode = rte.bits.delivery_mode;
		level = (rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL);
		vector = rte.bits.vector;
		dest = rte.bits.dest_field;

		len = snprintf(str, size, "\r\n%hhu\t0x%X\t%s\t0x%X\t%s\t%u\t%d\t%d",
				gsi, vector, phys ? "phys" : "logic", dest, level ? "level" : "edge",
				delmode >> 8U, remote_irr, mask);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
	}
END:
	snprintf(str, size, "\r\n");
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}

static int32_t shell_show_vioapic_info(int32_t argc, char **argv)
{
	uint16_t vmid;
	int32_t ret;

	/* User input invalidation */
	if (argc != 2) {
		return -EINVAL;
	}
	ret = strtol_deci(argv[1]);
	if (ret >= 0) {
		vmid = sanitize_vmid((uint16_t) ret);
		get_vioapic_info(shell_log_buf, SHELL_LOG_BUF_SIZE, vmid);
		shell_puts(shell_log_buf);
		return 0;
	}

	return -EINVAL;
}

/**
 * @brief Get information of ioapic
 *
 * It's for debug only.
 *
 * @param[in]	str_max_len	The max size of the string containing
 *				interrupt info
 * @param[inout]	str_arg	Pointer to the output information
 */
static int32_t get_ioapic_info(char *str_arg, size_t str_max_len)
{
	char *str = str_arg;
	uint32_t gsi;
	size_t len, size = str_max_len;
	uint32_t ioapic_nr_gsi = 0U;

	len = snprintf(str, size, "\r\nIRQ\tPIN\tRTE.HI32\tRTE.LO32\tVEC\tDST\tDM\tTM\tDELM\tIRR\tMASK");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	ioapic_nr_gsi = get_max_nr_gsi ();
	for (gsi = 0U; gsi < ioapic_nr_gsi; gsi++) {
		void *addr;
		uint32_t pin;
		union ioapic_rte rte;

		if (!is_gsi_valid(gsi)) {
			continue;
		}
		addr = gsi_to_ioapic_base(gsi);
		pin = gsi_to_ioapic_pin(gsi);

		ioapic_get_rte_entry(addr, pin, &rte);

		len = snprintf(str, size, "\r\n%03d\t%03hhu\t0x%08X\t0x%08X\t", gsi, pin, rte.u.hi_32, rte.u.lo_32);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;

		len = snprintf(str, size, "0x%02X\t0x%02X\t%s\t%s\t%u\t%d\t%d",
			rte.bits.vector, rte.bits.dest_field,
			(rte.bits.dest_mode == IOAPIC_RTE_DESTMODE_LOGICAL)? "logic" : "phys",
			(rte.bits.trigger_mode == IOAPIC_RTE_TRGRMODE_LEVEL)? "level" : "edge",
			rte.bits.delivery_mode, rte.bits.remote_irr,
			rte.bits.intr_mask);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;
	}

	snprintf(str, size, "\r\n");
	return 0;

overflow:
	printf("buffer size could not be enough! please check!\n");
	return 0;
}

static int32_t shell_show_ioapic_info(__unused int32_t argc, __unused char **argv)
{
	int32_t err = 0;

	err = get_ioapic_info(shell_log_buf, SHELL_LOG_BUF_SIZE);
	shell_puts(shell_log_buf);

	return err;
}

static int32_t shell_loglevel(int32_t argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};

	switch (argc) {
	case 4:
		npk_loglevel = (uint16_t)strtol_deci(argv[3]);
		/* falls through */
	case 3:
		mem_loglevel = (uint16_t)strtol_deci(argv[2]);
		/* falls through */
	case 2:
		console_loglevel = (uint16_t)strtol_deci(argv[1]);
		break;
	case 1:
		snprintf(str, MAX_STR_SIZE, "console_loglevel: %u, "
			"mem_loglevel: %u, npk_loglevel: %u\r\n",
			console_loglevel, mem_loglevel, npk_loglevel);
		shell_puts(str);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int32_t shell_cpuid(int32_t argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};
	uint32_t leaf, subleaf = 0;
	uint32_t eax, ebx, ecx, edx;

	if (argc == 2) {
		leaf = (uint32_t)strtoul_hex(argv[1]);
	} else if (argc == 3) {
		leaf = (uint32_t)strtoul_hex(argv[1]);
		subleaf = (uint32_t)strtoul_hex(argv[2]);
	} else {
		shell_puts("Please enter correct cmd with "
			"cpuid <leaf> [subleaf]\r\n");
		return -EINVAL;
	}

	cpuid_subleaf(leaf, subleaf, &eax, &ebx, &ecx, &edx);
	snprintf(str, MAX_STR_SIZE,
		"cpuid leaf: 0x%x, subleaf: 0x%x, 0x%x:0x%x:0x%x:0x%x\r\n",
		leaf, subleaf, eax, ebx, ecx, edx);

	shell_puts(str);

	return 0;
}

static int32_t shell_reboot(__unused int32_t argc, __unused char **argv)
{
	reset_host(false);
	return 0;
}

static int32_t shell_rdmsr(int32_t argc, char **argv)
{
	uint16_t pcpu_id = 0;
	int32_t ret = 0;
	uint32_t msr_index = 0;
	uint64_t val = 0;
	char str[MAX_STR_SIZE] = {0};

	pcpu_id = get_pcpu_id();

	switch (argc) {
	case 3:
		/* rdrmsr -p<PCPU_ID> <MSR_INDEX>*/
		 if ((argv[1][0] == '-') && (argv[1][1] == 'p')) {
			pcpu_id = (uint16_t)strtol_deci(&(argv[1][2]));
			msr_index = (uint32_t)strtoul_hex(argv[2]);
		} else {
			ret = -EINVAL;
		}
		break;
	case 2:
		/* rdmsr <MSR_INDEX> */
		msr_index = (uint32_t)strtoul_hex(argv[1]);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret == 0) {
		if (pcpu_id < get_pcpu_nums()) {
			val = msr_read_pcpu(msr_index, pcpu_id);
			snprintf(str, MAX_STR_SIZE, "rdmsr(0x%x):0x%lx\n", msr_index, val);
			shell_puts(str);
		} else {
			shell_puts("pcpu id is out of range!\n");
		}
	}

	return ret;
}

static int32_t shell_wrmsr(int32_t argc, char **argv)
{
	uint16_t pcpu_id = 0;
	int32_t ret = 0;
	uint32_t msr_index = 0;
	uint64_t val = 0;

	pcpu_id = get_pcpu_id();

	switch (argc) {
	case 4:
		/* wrmsr -p<PCPU_ID> <MSR_INDEX> <VALUE>*/
		 if ((argv[1][0] == '-') && (argv[1][1] == 'p')) {
			pcpu_id = (uint16_t)strtol_deci(&(argv[1][2]));
			msr_index = (uint32_t)strtoul_hex(argv[2]);
			val = strtoul_hex(argv[3]);
		} else {
			ret = -EINVAL;
		}
		break;
	case 3:
		/* wrmsr <MSR_INDEX> <VALUE>*/
		msr_index = (uint32_t)strtoul_hex(argv[1]);
		val = strtoul_hex(argv[2]);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret == 0) {
		if (pcpu_id < get_pcpu_nums()) {
			msr_write_pcpu(msr_index, val, pcpu_id);
		} else {
			shell_puts("pcpu id is out of range!\n");
		}
	}

	return ret;
}
