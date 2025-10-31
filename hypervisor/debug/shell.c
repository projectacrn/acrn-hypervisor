/*
 * Copyright (C) 2018-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <errno.h>
#include <bits.h>
#include "shell_priv.h"
#include <console.h>
#include <per_cpu.h>
#include <sprintf.h>
#include <logmsg.h>
#include <version.h>
#include <shell.h>
#include <cpu.h>

#define SHELL_PROMPT_STR	"ACRN:\\>"

char shell_log_buf[SHELL_LOG_BUF_SIZE];

extern struct shell_cmd arch_shell_cmds[];
extern uint32_t arch_shell_cmds_sz;
/* Input Line Other - Switch to the "other" input line (there are only two
 * input lines total).
 */

static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv);
static int32_t shell_version(__unused int32_t argc, __unused char **argv);
static int32_t shell_loglevel(int32_t argc, char **argv);
static int32_t shell_dump_host_mem(int32_t argc, char **argv);
static int32_t shell_list_vm(__unused int32_t argc, __unused char **argv);
static int32_t shell_list_vcpu(__unused int32_t argc, __unused char **argv);

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
		.str		= SHELL_CMD_LOG_LVL,
		.cmd_param	= SHELL_CMD_LOG_LVL_PARAM,
		.help_str	= SHELL_CMD_LOG_LVL_HELP,
		.fcn		= shell_loglevel,
	},
	{
		.str		= SHELL_CMD_DUMP_HOST_MEM,
		.cmd_param	= SHELL_CMD_DUMP_HOST_MEM_PARAM,
		.help_str	= SHELL_CMD_DUMP_HOST_MEM_HELP,
		.fcn		= shell_dump_host_mem,
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

extern uint16_t mem_loglevel;
extern uint16_t console_loglevel;
extern uint16_t npk_loglevel;


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
	for (i = 0U; i < p_shell->arch_cmd_count; i++) {
		p_cmd = &p_shell->arch_cmds[i];
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

void shell_puts(const char *string_ptr)
{
	/* Output the string */
	(void)console_write(string_ptr, strnlen_s(string_ptr,
				SHELL_STRING_MAX_LEN));
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

	p_shell->arch_cmds = arch_shell_cmds;
	p_shell->arch_cmd_count = arch_shell_cmds_sz;

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
	uint32_t cmd_cnt = p_shell->cmd_count + p_shell->arch_cmd_count;
	/* Print title */
	shell_puts("\r\nRegistered Commands:\r\n\r\n");

	pr_dbg("shell: Number of registered commands = %u in %s\n",
		cmd_cnt, __func__);

	/* Proceed based on the number of registered commands. */
	if (cmd_cnt == 0U) {
		/* No registered commands */
		shell_puts("NONE\r\n");
	} else {
		int32_t i = 0;
		uint32_t j;

		for (j = 0U; j < cmd_cnt; j++) {
			if (j < p_shell->cmd_count) {
				p_cmd = &p_shell->cmds[j];
			} else {
				p_cmd = &p_shell->arch_cmds[j - p_shell->cmd_count];
			}

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
