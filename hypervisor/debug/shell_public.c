/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include "shell_internal.h"

/* Shell that uses serial I/O */
static struct shell *serial_session;

static struct shell_cmd acrn_cmd[] = {
	{
		.str		= SHELL_CMD_HELP,
		.cmd_param	= SHELL_CMD_HELP_PARAM,
		.help_str	= SHELL_CMD_HELP_HELP,
		.fcn		= shell_cmd_help,
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
		.str		= SHELL_CMD_VCPU_PAUSE,
		.cmd_param	= SHELL_CMD_VCPU_PAUSE_PARAM,
		.help_str	= SHELL_CMD_VCPU_PAUSE_HELP,
		.fcn		= shell_pause_vcpu,
	},
	{
		.str		= SHELL_CMD_VCPU_RESUME,
		.cmd_param	= SHELL_CMD_VCPU_RESUME_PARAM,
		.help_str	= SHELL_CMD_VCPU_RESUME_HELP,
		.fcn		= shell_resume_vcpu,
	},
	{
		.str		= SHELL_CMD_VCPU_DUMPREG,
		.cmd_param	= SHELL_CMD_VCPU_DUMPREG_PARAM,
		.help_str	= SHELL_CMD_VCPU_DUMPREG_HELP,
		.fcn		= shell_vcpu_dumpreg,
	},
	{
		.str		= SHELL_CMD_VCPU_DUMPMEM,
		.cmd_param	= SHELL_CMD_VCPU_DUMPMEM_PARAM,
		.help_str	= SHELL_CMD_VCPU_DUMPMEM_HELP,
		.fcn		= shell_vcpu_dumpmem,
	},
	{
		.str		= SHELL_CMD_VM_CONSOLE,
		.cmd_param	= SHELL_CMD_VM_CONSOLE_PARAM,
		.help_str	= SHELL_CMD_VM_CONSOLE_HELP,
		.fcn		= shell_to_sos_console,
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
		.str		= SHELL_CMD_REQ,
		.cmd_param	= SHELL_CMD_REQ_PARAM,
		.help_str	= SHELL_CMD_REQ_HELP,
		.fcn		= shell_show_req_info,
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
		.str		= SHELL_CMD_VMEXIT,
		.cmd_param	= SHELL_CMD_VMEXIT_PARAM,
		.help_str	= SHELL_CMD_VMEXIT_HELP,
		.fcn		= shell_show_vmexit_profile,
	},
	{
		.str		= SHELL_CMD_LOGDUMP,
		.cmd_param	= SHELL_CMD_LOGDUMP_PARAM,
		.help_str	= SHELL_CMD_LOGDUMP_HELP,
		.fcn		= shell_dump_logbuf,
	},
	{
		.str		= SHELL_CMD_GET_LOG_LVL,
		.cmd_param	= SHELL_CMD_GET_LOG_LVL_PARAM,
		.help_str	= SHELL_CMD_GET_LOG_LVL_HELP,
		.fcn		= shell_get_loglevel,
	},
	{
		.str		= SHELL_CMD_SET_LOG_LVL,
		.cmd_param	= SHELL_CMD_SET_LOG_LVL_PARAM,
		.help_str	= SHELL_CMD_SET_LOG_LVL_HELP,
		.fcn		= shell_set_loglevel,
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
			.str		= SHELL_CMD_TRIGGER_CRASH,
			.cmd_param	= SHELL_CMD_TRIGGER_CRASH_PARAM,
			.help_str	= SHELL_CMD_TRIGGER_CRASH_HELP,
			.fcn		= shell_trigger_crash,
		},
};

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

	serial_session->shell_cmd = acrn_cmd;
	serial_session->cmd_count = ARRAY_SIZE(acrn_cmd);
	/* Initialize the handler for the serial port that will be used
	 * for shell i/p and o/p
	 */
	status = serial_session->session_io.io_init(serial_session);

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
