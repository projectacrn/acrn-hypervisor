/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hypervisor.h>
#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include "shell_internal.h"

/* Shell that uses serial I/O */
static struct shell *serial_session;

static int shell_register_cmd(struct shell *p_shell,
			const char *cmd,
			const char *cmd_param,
			const char *cmd_help_str,
			int (*cmd_fcn)(struct shell *, int, char **))
{
	int status = 0;
	struct shell_cmd *p_cmd;
	uint32_t cmd_mem_size;

	if ((p_shell == NULL) || (cmd == NULL) ||
		(cmd_help_str == NULL) || (cmd_fcn == NULL)) {
		return -EINVAL;
	}

	/* Check if a duplicate command exists */
	p_cmd = shell_find_cmd(p_shell, cmd);
	if (p_cmd != NULL) {
		/* Requested command is already registered */
		pr_err("Error: Command %s is already registered.", cmd);
		status = -EINVAL;
		goto exit;
	}

	/* Requested command is not already registered. So allocate enough
	 * memory for the command structure and the command, parameter and the
	 * help text strings along with the corresponding null terminating
	 * character/s.
	 */
	cmd_mem_size = sizeof(struct shell_cmd)
			+ (strnlen_s(cmd, SHELL_CMD_MAX_LEN) + 1);

	/* If command takes any parameters, need to allocate memory for storing
	 * parameter string.
	 */
	if (cmd_param)
		cmd_mem_size += strnlen_s(cmd_param, SHELL_PARA_MAX_LEN) + 1;

	/* If help text is provided for command, need to allocate memory for
	 * storing help string.
	 */
	if (cmd_help_str)
		cmd_mem_size += strnlen_s(cmd_help_str, SHELL_HELP_MAX_LEN) + 1;

	p_cmd = (struct shell_cmd *) calloc(1, cmd_mem_size);
	if (p_cmd == NULL) {
		status = -ENOMEM;
		goto exit;
	}

	/* The command structure, command string, it's parameter string and
	 * the associated help string are all stored in contiguous memory
	 * locations. So the cmd string immediately follows the command
	 * structure..
	 */
	p_cmd->str = (char *)p_cmd + sizeof(struct shell_cmd);
	strncpy_s(p_cmd->str, SHELL_CMD_MAX_LEN, cmd, SHELL_CMD_MAX_LEN);

	/* Check if this command does take any parameters... */
	if (cmd_param) {
		/* The command parameter string immediately follows the command
		 * string in memory.
		 */
		p_cmd->cmd_param = p_cmd->str
			+ (strnlen_s(cmd, SHELL_CMD_MAX_LEN) + 1);
		strcpy_s(p_cmd->cmd_param, SHELL_PARA_MAX_LEN, cmd_param);
	}

	/* Check if help string is provided for the command.. */
	if (cmd_help_str) {
		if (cmd_param) {
			/* The command help string immediately follows the
			 * parameter string in memory | cmd_structure |
			 * cmd_str | param_str | help_str |
			 */
			p_cmd->help_str = p_cmd->cmd_param +
				(strnlen_s(cmd_param, SHELL_PARA_MAX_LEN) + 1);

			strcpy_s(p_cmd->help_str,
					SHELL_HELP_MAX_LEN, cmd_help_str);
		} else {
			/* No command parameter/s. Help string immediately
			 * follows the cmd string | cmd_structure | cmd_str |
			 * help_str |
			 */
			p_cmd->help_str = p_cmd->str +
				(strnlen_s(cmd, SHELL_CMD_MAX_LEN) + 1);

			strcpy_s(p_cmd->help_str,
					SHELL_HELP_MAX_LEN, cmd_help_str);
		}
	}

	/* Set the command function. */
	p_cmd->fcn = cmd_fcn;

	INIT_LIST_HEAD(&p_cmd->node);
	list_add(&p_cmd->node, &p_shell->cmd_list);

	/* Update command count. */
	p_shell->cmd_count++;

	status = 0;

exit:
	return status;
}

int shell_init(void)
{
	int status;

	status = shell_construct(&serial_session);
	if (status != 0)
		return status;

	/* Set the function pointers for the shell i/p and o/p functions */
	serial_session->session_io.io_init = shell_init_serial;
	serial_session->session_io.io_deinit = shell_terminate_serial;
	serial_session->session_io.io_puts = shell_puts_serial;
	serial_session->session_io.io_getc = shell_getc_serial;
	serial_session->session_io.io_special = shell_special_serial;
	serial_session->session_io.io_echo_on = (bool)true;

	/* Initialize the handler for the serial port that will be used
	 * for shell i/p and o/p
	 */
	status = serial_session->session_io.io_init(serial_session);

	/* Register command handlers for the shell commands that are available
	 * by default
	 */
	if (status == 0) {
		status = shell_register_cmd(serial_session,
					SHELL_CMD_HELP,
					SHELL_CMD_HELP_PARAM,
					SHELL_CMD_HELP_HELP,
					shell_cmd_help);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_HELP);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VM_LIST,
					SHELL_CMD_VM_LIST_PARAM,
					SHELL_CMD_VM_LIST_HELP,
					shell_list_vm);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VM_LIST);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VCPU_LIST,
					SHELL_CMD_VCPU_LIST_PARAM,
					SHELL_CMD_VCPU_LIST_HELP,
					shell_list_vcpu);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VCPU_LIST);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VCPU_PAUSE,
					SHELL_CMD_VCPU_PAUSE_PARAM,
					SHELL_CMD_VCPU_PAUSE_HELP,
					shell_pause_vcpu);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VCPU_PAUSE);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VCPU_RESUME,
					SHELL_CMD_VCPU_RESUME_PARAM,
					SHELL_CMD_VCPU_RESUME_HELP,
					shell_resume_vcpu);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VCPU_RESUME);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VCPU_DUMPREG,
					SHELL_CMD_VCPU_DUMPREG_PARAM,
					SHELL_CMD_VCPU_DUMPREG_HELP,
					shell_vcpu_dumpreg);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VCPU_DUMPREG);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VCPU_DUMPMEM,
					SHELL_CMD_VCPU_DUMPMEM_PARAM,
					SHELL_CMD_VCPU_DUMPMEM_HELP,
					shell_vcpu_dumpmem);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VCPU_DUMPMEM);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VM_CONSOLE,
					SHELL_CMD_VM_CONSOLE_PARAM,
					SHELL_CMD_VM_CONSOLE_HELP,
					shell_to_sos_console);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VM_CONSOLE);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_INTERRUPT,
					SHELL_CMD_INTERRUPT_PARAM,
					SHELL_CMD_INTERRUPT_HELP,
					shell_show_cpu_int);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_INTERRUPT);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_PTDEV,
					SHELL_CMD_PTDEV_PARAM,
					SHELL_CMD_PTDEV_HELP,
					shell_show_ptdev_info);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_PTDEV);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_REQ,
					SHELL_CMD_REQ_PARAM,
					SHELL_CMD_REQ_HELP,
					shell_show_req_info);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_REQ);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VIOAPIC,
					SHELL_CMD_VIOAPIC_PARAM,
					SHELL_CMD_VIOAPIC_HELP,
					shell_show_vioapic_info);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VIOAPIC);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_IOAPIC,
					SHELL_CMD_IOAPIC_PARAM,
					SHELL_CMD_IOAPIC_HELP,
					shell_show_ioapic_info);
		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_IOAPIC);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_VMEXIT,
					SHELL_CMD_VMEXIT_PARAM,
					SHELL_CMD_VMEXIT_HELP,
					shell_show_vmexit_profile);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_VMEXIT);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_LOGDUMP,
					SHELL_CMD_LOGDUMP_PARAM,
					SHELL_CMD_LOGDUMP_HELP,
					shell_dump_logbuf);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_LOGDUMP);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_GET_LOG_LVL,
					SHELL_CMD_GET_LOG_LVL_PARAM,
					SHELL_CMD_GET_LOG_LVL_HELP,
					shell_get_loglevel);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_GET_LOG_LVL);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_SET_LOG_LVL,
					SHELL_CMD_SET_LOG_LVL_PARAM,
					SHELL_CMD_SET_LOG_LVL_HELP,
					shell_set_loglevel);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_SET_LOG_LVL);
		}

		status = shell_register_cmd(serial_session,
					SHELL_CMD_CPUID,
					SHELL_CMD_CPUID_PARAM,
					SHELL_CMD_CPUID_HELP,
					shell_cpuid);

		if (status != 0) {
			pr_err("Error: Command \"%s\" registration failed.",
				SHELL_CMD_CPUID);
		}
	}

	return status;
}

int shell_puts(struct shell *p_shell, char *str_ptr)
{
	int status;

	if ((p_shell != NULL) && (p_shell->session_io.io_puts != NULL) &&
			(str_ptr != NULL)) {
		/* Transmit data using this shell session's 'puts' function */
		p_shell->session_io.io_puts(p_shell, str_ptr);

		status = 0;
	} else {
		/* Error: Invalid request */
		status = -EINVAL;

	}

	return status;
}

int shell_set_name(struct shell *p_shell, char *name)
{
	int status;

	if ((p_shell != NULL) && (name != NULL)) {
		strncpy_s((void *) p_shell->name, SHELL_NAME_MAX_LEN,
			(void *) name, SHELL_NAME_MAX_LEN - 1);

		/* Ensure null terminated string */
		p_shell->name[SHELL_NAME_MAX_LEN - 1] = 0;

		status = 0;
	} else {
		status = -EINVAL;
	}

	return status;
}

void shell_kick_session(void)
{
	/* Kick the shell */
	kick_shell(serial_session);
}

int shell_switch_console(void)
{
	struct vuart *vuart;

	vuart = vuart_console_active();
	if (vuart == NULL)
		return -EINVAL;

	vuart->active = false;
	/* Output that switching to ACRN shell */
	shell_puts(serial_session,
		"\r\n\r\n----- Entering ACRN Shell -----\r\n");
	return 0;
}
