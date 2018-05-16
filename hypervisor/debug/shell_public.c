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
