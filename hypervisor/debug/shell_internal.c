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

#include <hv_lib.h>
#include <acrn_common.h>
#include <hv_arch.h>
#include <hv_debug.h>
#include <hypercall.h>
#include "shell_internal.h"
#include "serial_internal.h"

#define TEMP_STR_SIZE		60
#define MAX_STR_SIZE		256

#define SHELL_PROMPT_STR	"ACRN:\\>"
#define NO_SERIAL_SHELL		-4252	/* No serial shell enabled */
#define KILL_SHELL		-4253	/* Ends processing of shell */

#define SHELL_CMD_VM_ID_ERROR_MESSAGE(CMD) \
	"Syntax: "CMD" <vm id> where <vm id> is ID of the vm."

/* ASCII Manipulation */
#define SHELL_ASCII_LOWER_CASE_OFFSET	32

/* Input Line Other - Switch to the "other" input line (there are only two
 * input lines total).
 */
#define SHELL_INPUT_LINE_OTHER(v)	(((v) + 1) % 2)

/* The initial log level*/
uint32_t console_loglevel;
uint32_t mem_loglevel;
#ifdef CONSOLE_LOGLEVEL_DEFAULT
uint32_t console_loglevel = CONSOLE_LOGLEVEL_DEFAULT;
#endif
#ifdef MEM_LOGLEVEL_DEFAULT
uint32_t mem_loglevel = MEM_LOGLEVEL_DEFAULT;
#endif

static int string_to_argv(char *argv_str, void *p_argv_mem,
		__unused uint32_t argv_mem_size, int *p_argc, char ***p_argv)
{
	uint32_t argc;
	char **argv;
	char *p_ch;

	/* Setup initial argument values. */
	argc = 0;
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
		while ((*p_ch != ' ') && (*p_ch != ',') && (*p_ch != 0))
			p_ch++;

		/* Count the argument just processed. */
		argc++;

		/* Check for the end of the argument string. */
		if (*p_ch != 0) {
			/* Terminate the vector entry argument string
			 * and move to the next.
			 */
			*p_ch = 0;
			/* Remove all space in middile of cmdline */
			while (*++p_ch == ' ')
				;
		}
	}

	/* Update return parameters */
	*p_argc = argc;
	*p_argv = argv;

	return 0;
}

static uint8_t shell_input_line(struct shell *p_shell)
{
	bool done = false;
	uint8_t ch;

	/* Get a character from the user. */
	ch = p_shell->session_io.io_getc(p_shell);

	/* Check character */
	switch (ch) {
	/* Backspace */
	case '\b':
		/* Ensure length is not 0 */
		if (p_shell->input_line_len > 0) {
			/* Reduce the length of the string by one */
			p_shell->input_line_len--;

			/* Null terminate the last character to erase it */
			p_shell->input_line[p_shell->input_line_active]
					[p_shell->input_line_len] = 0;

			if (p_shell->session_io.io_echo_on == true) {
				/* Echo backspace */
				p_shell->session_io.io_puts(p_shell, "\b");
			}

			/* Send a space + backspace sequence to delete
			 * character
			 */
			p_shell->session_io.io_puts(p_shell, " \b");
		} else if (p_shell->session_io.io_echo_on == false) {
			/* Put last character of prompt to prevent backspace
			 * in terminal
			 */
			p_shell->session_io.io_puts(p_shell, ">");
		}
		break;

	/* Carriage-return */
	case '\r':
		/* See if echo is on */
		if (p_shell->session_io.io_echo_on == true) {
			/* Echo carriage return / line feed */
			p_shell->session_io.io_puts(p_shell, "\r\n");
		}

		/* Set flag showing line input done */
		done = true;

		/* Reset command length for next command processing */
		p_shell->input_line_len = 0;
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
				/* Add character to string */
				p_shell->input_line[p_shell->input_line_active]
						[p_shell->input_line_len] = ch;
				/* See if echo is on */
				if (p_shell->session_io.io_echo_on == true) {
					/* Echo back the input */
					p_shell->session_io.io_puts(p_shell,
							&p_shell->input_line
							[p_shell->input_line_active]
							[p_shell->input_line_len]);
				}

				/* Move to next character in string */
				p_shell->input_line_len++;
			} else {
				/* prINTable character */
				/* See if a "special" character handler is installed */
				if (p_shell->session_io.io_special) {
					/* Call special character handler */
					p_shell->session_io.io_special(p_shell, ch);
				}
			}
		} else {
			/* See if echo is on */
			if (p_shell->session_io.io_echo_on == true) {
				/* Echo carriage return / line feed */
				p_shell->session_io.io_puts(p_shell, "\r\n");
			}

			/* Set flag showing line input done */
			done = true;

			/* Reset command length for next command processing */
			p_shell->input_line_len = 0;

		}
		break;
	}


	return done;
}

static int shell_process(struct shell *p_shell)
{
	int status;
	char *p_input_line;

	/* Check for the repeat command character in active input line.
	 */
	if (p_shell->input_line[p_shell->input_line_active][0] == '.') {
		/* Repeat the last command (using inactive input line).
		 */
		p_input_line =
			&p_shell->input_line[SHELL_INPUT_LINE_OTHER
				(p_shell->input_line_active)][0];
	} else {
		/* Process current command (using active input line). */
		p_input_line =
			&p_shell->input_line[p_shell->input_line_active][0];

		/* Switch active input line. */
		p_shell->input_line_active =
			SHELL_INPUT_LINE_OTHER(p_shell->input_line_active);
	}

	/* Process command */
	status = shell_process_cmd(p_shell, p_input_line);

	/* Now that the command is processed, zero fill the input buffer */
	memset((void *) p_shell->input_line[p_shell->input_line_active], 0,
		SHELL_CMD_MAX_LEN + 1);

	/* Process command and return result to caller */
	return status;
}

struct shell_cmd *shell_find_cmd(struct shell *p_shell, const char *cmd_str)
{
	uint32_t i;
	struct shell_cmd *p_cmd = NULL;

	if (p_shell->cmd_count <= 0)
		return NULL;

	for (i = 0; i < p_shell->cmd_count; i++) {
		p_cmd = &p_shell->shell_cmd[i];
		if (strcmp(p_cmd->str, cmd_str) == 0)
			break;
	}

	if (i == p_shell->cmd_count) {
		/* No commands in the list. */
		p_cmd = NULL;
	}

	return p_cmd;
}

void kick_shell(struct shell *p_shell)
{
	int status = p_shell ? 0 : EINVAL;
	static uint8_t is_cmd_cmplt = 1;

	if (status == 0) {
		pr_dbg("shell: entering the shell cmd "
			"handling loop from function %s\n", __func__);

		/* At any given instance, UART may be owned by the HV
		 * OR by the guest that has enabled the vUart.
		 * Show HV shell prompt ONLY when HV owns the
		 * serial port.
		 */
		if (!vuart_console_active()) {
			/* Prompt the user for a selection. */
			if (is_cmd_cmplt && p_shell->session_io.io_puts)
				p_shell->session_io.io_puts(p_shell,
							SHELL_PROMPT_STR);

			/* Get user's input */
			is_cmd_cmplt = shell_input_line(p_shell);

			/* If user has pressed the ENTER then process
			 * the command
			 */
			if (is_cmd_cmplt)
				/* Process current input line. */
				status = shell_process(p_shell);
		}
	} else {
		/* Serial port handle couldn't be obtained. Stop the shell
		 * task.
		 */
		pr_info("shell: stopping the shell task...");
	}
}

int shell_process_cmd(struct shell *p_shell, char *p_input_line)
{
	int status = 0;
	struct shell_cmd *p_cmd;
	shell_cmd_fn_t cmd_fcn;
	char cmd_argv_str[SHELL_CMD_MAX_LEN + 1];
	int cmd_argv_mem[sizeof(char *) * ((SHELL_CMD_MAX_LEN + 1) / 2)];
	int cmd_argc;
	char **cmd_argv;

	/* Copy the input line INTo an argument string to become part of the
	 * argument vector.
	 */
	(void) strcpy_s(&cmd_argv_str[0], SHELL_CMD_MAX_LEN, p_input_line);
	cmd_argv_str[SHELL_CMD_MAX_LEN] = 0;

	/* Build the argv vector from the string. The first argument in the
	 * resulting vector will be the command string itself.
	 */

	/* NOTE: This process is destructive to the argument string! */

	(void) string_to_argv(&cmd_argv_str[0],
			(void *) &cmd_argv_mem[0],
			sizeof(cmd_argv_mem), &cmd_argc, &cmd_argv);

	/* Determine if there is a command to process. */
	if (cmd_argc != 0) {
		/* See if command is in cmds supported */
		p_cmd = shell_find_cmd(p_shell, cmd_argv[0]);

		if (p_cmd != NULL) {
			/* Make a copy of the command function to in case it is
			 * removed right before the call.
			 */
			cmd_fcn = p_cmd->fcn;

			/* Call the command passing the appropriate command
			 * arguments.
			 */
			status = cmd_fcn(p_shell, cmd_argc, &cmd_argv[0]);
		} else {	/* unregistered cmd */
			p_shell->session_io.io_puts(p_shell,
				"\r\nError: Invalid Command\r\n\r\n");
		}
	}

	return status;
}

int shell_init_serial(struct shell *p_shell)
{
	int status = 0;

	uint32_t serial_handle = get_serial_handle();

	if (serial_handle != SERIAL_INVALID_HANDLE) {
		p_shell->session_io.io_session_info =
				(void *)(uint64_t)serial_handle;

		status = shell_set_name(p_shell, "Serial");
	} else {
		status = NO_SERIAL_SHELL;
		pr_err("Error: Unable to get a valid serial port handle");
	}

	/* Zero fill the input buffer */
	memset((void *)p_shell->input_line[p_shell->input_line_active], 0,
			SHELL_CMD_MAX_LEN + 1);

	return status;
}

#define SHELL_ROWS	10
#define MAX_INDENT_LEN	16
int shell_cmd_help(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	int status = 0;
	int spaces = 0;
	struct shell_cmd *p_cmd = NULL;
	char space_buf[MAX_INDENT_LEN + 1];

	/* Print title */
	shell_puts(p_shell, "\r\nRegistered Commands:\r\n\r\n");

	pr_dbg("shell: Number of registered commands = %u in %s\n",
		p_shell->cmd_count, __func__);

	memset(space_buf, ' ', sizeof(space_buf));
	/* Proceed based on the number of registered commands. */
	if (p_shell->cmd_count == 0) {
		/* No registered commands */
		shell_puts(p_shell, "NONE\r\n");
	} else {
		int i = 0;
		uint32_t j;

		for (j = 0; j < p_shell->cmd_count; j++) {
			p_cmd = &p_shell->shell_cmd[j];

			/* Check if we've filled the screen with info */
			/* i + 1 used to avoid 0%SHELL_ROWS=0 */
			if (((i + 1) % SHELL_ROWS) == 0) {
				/* Pause before we continue on to the next
				 * page.
				 */

				/* Print message to the user. */
				shell_puts(p_shell,
					"<**** Hit any key to continue ****>");

				/* Wait for a character from user (NOT USED) */
				(void)p_shell->session_io.io_getc(p_shell);

				/* Print a new line after the key is hit. */
				shell_puts(p_shell, "\r\n");
			}

			i++;

			/* Output the command string */
			shell_puts(p_shell, "  ");
			shell_puts(p_shell, p_cmd->str);

			/* Calculate spaces needed for alignment */
			spaces = MAX_INDENT_LEN - strnlen_s(p_cmd->str,
					MAX_INDENT_LEN - 1);

			space_buf[spaces] = '\0';
			shell_puts(p_shell, space_buf);
			space_buf[spaces] = ' ';

			/* Display parameter info if applicable. */
			if (p_cmd->cmd_param) {
				shell_puts(p_shell, p_cmd->cmd_param);
			}

			/* Display help text if available. */
			if (p_cmd->help_str) {
				shell_puts(p_shell, " - ");
				shell_puts(p_shell, p_cmd->help_str);
			}
			shell_puts(p_shell, "\r\n");
		}
	}

	shell_puts(p_shell, "\r\n");

	return status;
}

int shell_list_vm(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	int status = 0;
	char temp_str[MAX_STR_SIZE];
	struct list_head *pos;
	struct vm *vm;

	shell_puts(p_shell,
		"\r\nVM NAME                  VM ID            VM STATE"
		"\r\n=======                  =====            ========\r\n");

	spinlock_obtain(&vm_list_lock);
	list_for_each(pos, &vm_list) {
		char state[32];

		vm = list_entry(pos, struct vm, list);
		switch (vm->state) {
		case VM_CREATED:
			strcpy_s(state, 32, "Created"); break;
		case VM_STARTED:
			strcpy_s(state, 32, "Started"); break;
		case VM_PAUSED:
			strcpy_s(state, 32, "Paused"); break;
		default:
			strcpy_s(state, 32, "Unknown"); break;
		}
		/* Create output string consisting of VM name and VM id
		 */
		snprintf(temp_str, MAX_STR_SIZE,
				"%-24s %-16d %-8s\r\n", vm->attr.name,
				vm->attr.id, state);

		/* Output information for this task */
		shell_puts(p_shell, temp_str);
	}
	spinlock_release(&vm_list_lock);

	return status;
}

int shell_list_vcpu(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	int status = 0;
	char temp_str[MAX_STR_SIZE];
	struct list_head *pos;
	struct vm *vm;
	struct vcpu *vcpu;

	shell_puts(p_shell,
		"\r\nVM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE"
		"\r\n=====    =======    =======    =========    ==========\r\n");

	spinlock_obtain(&vm_list_lock);
	list_for_each(pos, &vm_list) {
		char state[32];
		int i;

		vm = list_entry(pos, struct vm, list);
		foreach_vcpu(i, vm, vcpu) {
			switch (vcpu->state) {
			case VCPU_INIT:
				strcpy_s(state, 32, "Init"); break;
			case VCPU_PAUSED:
				strcpy_s(state, 32, "Paused"); break;
			case VCPU_RUNNING:
				strcpy_s(state, 32, "Running"); break;
			case VCPU_ZOMBIE:
				strcpy_s(state, 32, "Zombie"); break;
			default:
				strcpy_s(state, 32, "Unknown");
			}
			/* Create output string consisting of VM name
			 * and VM id
			 */
			snprintf(temp_str, MAX_STR_SIZE,
					"  %-9d %-10d %-7d %-12s %-16s\r\n",
					vm->attr.id,
					vcpu->pcpu_id,
					vcpu->vcpu_id,
					is_vcpu_bsp(vcpu) ?
					"PRIMARY" : "SECONDARY",
					state);
			/* Output information for this task */
			shell_puts(p_shell, temp_str);
		}
	}
	spinlock_release(&vm_list_lock);

	return status;
}

int shell_pause_vcpu(struct shell *p_shell,
		int argc, char **argv)
{
	int status = 0;
	uint32_t vm_id, vcpu_id;
	struct vm *vm;
	struct vcpu *vcpu;

	/* User input invalidation */
	if (argc != 3) {
		status = -EINVAL;
		shell_puts(p_shell,
			"Please enter correct cmd with <vm_id, vcpu_id>\r\n");
	} else {
		vm_id = atoi(argv[1]);
		vcpu_id = atoi(argv[2]);

		vm = get_vm_from_vmid(vm_id);
		if (vm) {
			vcpu = vcpu_from_vid(vm, vcpu_id);
			if (vcpu) {
				if (vcpu->dbg_req_state != VCPU_PAUSED) {
					vcpu->dbg_req_state = VCPU_PAUSED;
					/* TODO: do we need file a IPI to kick
					 * VCPU immediately */
					shell_puts(p_shell,
						"The vcpu will PAUSE in "
						"next vm exit\r\n");
				} else {
					shell_puts(p_shell,
					"Request again, do nothing\r\n");
				}
			} else {
				status = -EINVAL;
				shell_puts(p_shell,
					"No vcpu found in the input "
					"<vm_id, vcpu_id>\r\n");
			}
		} else {
			status = -EINVAL;
			shell_puts(p_shell,
					"No vm found in the input "
					"<vm_id, vcpu_id>\r\n");
		}
	}

	return status;
}

int shell_resume_vcpu(struct shell *p_shell,
		int argc, char **argv)
{
	int status = 0;
	uint32_t vm_id, vcpu_id;
	struct vm *vm;
	struct vcpu *vcpu;

	/* User input invalidation */
	if (argc != 3) {
		status = -EINVAL;
		shell_puts(p_shell,
			"Please enter correct cmd with <vm_id, vcpu_id>\r\n");
	} else {
		vm_id = atoi(argv[1]);
		vcpu_id = atoi(argv[2]);
		vm = get_vm_from_vmid(vm_id);
		if (vm) {
			vcpu = vcpu_from_vid(vm, vcpu_id);
			if (vcpu) {
				if (vcpu->dbg_req_state == VCPU_PAUSED) {
					vcpu->dbg_req_state = 0;
					shell_puts(p_shell,
						"The vcpu resummed\r\n");
				} else {
					shell_puts(p_shell,
						"vcpu is not in debug PAUSE, "
						"do nothing\r\n");
				}
			} else {
				status = -EINVAL;
				shell_puts(p_shell,
					"No vcpu found in the input "
					"<vm_id, vcpu_id>\r\n");
			}
		} else {
			status = -EINVAL;
			shell_puts(p_shell,
				"No vm found in the input "
				"<vm_id, vcpu_id>\r\n");
		}
	}

	return status;
}

int shell_vcpu_dumpreg(struct shell *p_shell,
		int argc, char **argv)
{
	int status = 0;
	uint32_t vm_id, vcpu_id;
	char temp_str[MAX_STR_SIZE];
	struct vm *vm;
	struct vcpu *vcpu;
	uint64_t gpa, hpa, i;
	uint64_t *tmp;
	struct run_context *cur_context;

	/* User input invalidation */
	if (argc != 3) {
		shell_puts(p_shell,
			"Please enter correct cmd with <vm_id, vcpu_id>\r\n");
		return -EINVAL;
	}

	vm_id = atoi(argv[1]);
	vcpu_id = atoi(argv[2]);

	vm = get_vm_from_vmid(vm_id);
	if (!vm) {
		shell_puts(p_shell, "No vm found in the input "
				"<vm_id, vcpu_id>\r\n");
		return -EINVAL;
	}

	vcpu = vcpu_from_vid(vm, vcpu_id);
	if (!vcpu) {
		shell_puts(p_shell, "No vcpu found in the input "
				"<vm_id, vcpu_id>\r\n");
		return -EINVAL;
	}

	cur_context = &vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

	if (vcpu->state != VCPU_PAUSED) {
		shell_puts(p_shell, "NOTE: VCPU unPAUSEed, regdump "
				"may not be accurate\r\n");
	}

	snprintf(temp_str, MAX_STR_SIZE,
		"=  VM ID %d ==== CPU ID %d========================\r\n",
		vm->attr.id, vcpu->vcpu_id);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RIP=0x%016llx  RSP=0x%016llx "
			"RFLAGS=0x%016llx\r\n", cur_context->rip,
			cur_context->rsp, cur_context->rflags);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  CR0=0x%016llx  CR2=0x%016llx "
			" CR3=0x%016llx\r\n", cur_context->cr0,
			cur_context->cr2, cur_context->cr3);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RAX=0x%016llx  RBX=0x%016llx  "
			"RCX=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.rax,
			cur_context->guest_cpu_regs.regs.rbx,
			cur_context->guest_cpu_regs.regs.rcx);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RDX=0x%016llx  RDI=0x%016llx  "
			"RSI=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.rdx,
			cur_context->guest_cpu_regs.regs.rdi,
			cur_context->guest_cpu_regs.regs.rsi);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RBP=0x%016llx  R8=0x%016llx  "
			"R9=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.rbp,
			cur_context->guest_cpu_regs.regs.r8,
			cur_context->guest_cpu_regs.regs.r9);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  R10=0x%016llx  R11=0x%016llx  "
			"R12=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.r10,
			cur_context->guest_cpu_regs.regs.r11,
			cur_context->guest_cpu_regs.regs.r12);
	shell_puts(p_shell, temp_str);
	snprintf(temp_str, MAX_STR_SIZE,
			"=  R13=0x%016llx  R14=0x%016llx  R15=0x%016llx\r\n",
			cur_context->guest_cpu_regs.regs.r13,
			cur_context->guest_cpu_regs.regs.r14,
			cur_context->guest_cpu_regs.regs.r15);
	shell_puts(p_shell, temp_str);

	/* dump sp */
	gpa = gva2gpa(vm, cur_context->cr3,
			cur_context->rsp);
	if (gpa == 0) {
		status = -EINVAL;
		shell_puts(p_shell, "Cannot handle user gva yet!\r\n");
	} else {
		hpa = gpa2hpa(vm, gpa);
		snprintf(temp_str, MAX_STR_SIZE,
				"\r\nDump RSP for vm %d, from "
				"gva 0x%016llx -> gpa 0x%016llx"
				" -> hpa 0x%016llx:\r\n",
				vm_id, cur_context->rsp,gpa, hpa);
		shell_puts(p_shell, temp_str);

		tmp = HPA2HVA(hpa);
		for (i = 0; i < 8; i++) {
			snprintf(temp_str, MAX_STR_SIZE,
					"=  0x%016llx  0x%016llx  "
					"0x%016llx  0x%016llx\r\n",
					tmp[i*4], tmp[i*4+1],
					tmp[i*4+2], tmp[i*4+3]);
			shell_puts(p_shell, temp_str);
		}
	}

	return status;
}

int shell_vcpu_dumpmem(struct shell *p_shell,
		int argc, char **argv)
{
	int status = 0;
	uint32_t vm_id, vcpu_id;
	uint64_t gva, gpa, hpa;
	uint64_t *tmp;
	uint32_t i, length = 32;
	char temp_str[MAX_STR_SIZE];
	struct vm *vm;
	struct vcpu *vcpu;

	/* User input invalidation */
	if (argc != 4 && argc != 5) {
		status = -EINVAL;
		shell_puts(p_shell,
			"Please enter correct cmd with "
			"<vm_id, vcpu_id, gva, length>\r\n");
		return status;
	}

	vm_id = atoi(argv[1]);
	vcpu_id = atoi(argv[2]);

	vm = get_vm_from_vmid(vm_id);
	if (vm == NULL) {
		status = -EINVAL;
		shell_puts(p_shell,
		"No vm found in the input <vm_id, cpu_id, gva, length>\r\n");
		return status;
	}

	gva = strtoul(argv[3], NULL, 16);

	if (argc == 5)
		length = atoi(argv[4]);

	vcpu = vcpu_from_vid(vm, (long)vcpu_id);
	if (vcpu) {
		struct run_context *cur_context =
			&vcpu->arch_vcpu.contexts[vcpu->arch_vcpu.cur_context];

		gpa = gva2gpa(vcpu->vm, cur_context->cr3, gva);
		if (gpa == 0) {
			status = -EINVAL;
			shell_puts(p_shell,
					"Cannot handle user gva yet!\r\n");
		} else {
			hpa = gpa2hpa(vcpu->vm, gpa);
			snprintf(temp_str, MAX_STR_SIZE,
				"Dump memory for vcpu %d, from gva 0x%016llx ->"
				"gpa 0x%016llx -> hpa 0x%016llx, length "
				"%d:\r\n", vcpu_id, gva, gpa, hpa, length);
			shell_puts(p_shell, temp_str);

			tmp = HPA2HVA(hpa);
			for (i = 0; i < length/32; i++) {
				snprintf(temp_str, MAX_STR_SIZE,
					"=  0x%016llx  0x%016llx  0x%016llx  "
					"0x%016llx\r\n", tmp[i*4], tmp[i*4+1],
					tmp[i*4+2], tmp[i*4+3]);
				shell_puts(p_shell, temp_str);
			}
			if (length > 32*(length/32)) {
				snprintf(temp_str, MAX_STR_SIZE,
					"=  0x%016llx  0x%016llx  0x%016llx  "
					"0x%016llx\r\n", tmp[i*4], tmp[i*4+1],
					tmp[i*4+2], tmp[i*4+3]);
				shell_puts(p_shell, temp_str);
			}
		}
	} else {
		status = -EINVAL;
		shell_puts(p_shell,
			"No vcpu found in the input <vcpu_id, gva, lengthe>\r\n");
	}

	return status;
}

int shell_to_sos_console(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	char temp_str[TEMP_STR_SIZE];
	int guest_no = 0;

	struct vm *vm;
	struct vuart *vuart;

	/* Get the virtual device node */
	vm = get_vm_from_vmid(guest_no);
	if (vm == NULL) {
		pr_err("Error: VM %d is not yet created/started",
				guest_no);

		return -EINVAL;
	}
	vuart = vm->vuart;
	if (vuart == NULL) {
		snprintf(temp_str, TEMP_STR_SIZE,
				"\r\nError: serial console driver is not "
				"enabled for VM %d\r\n",
				guest_no);
		shell_puts(p_shell, temp_str);
	} else {
		/* UART is now owned by the SOS.
		 * Indicate by toggling the flag.
		 */
		vuart->active = true;
		/* Output that switching to SOS shell */
		snprintf(temp_str, TEMP_STR_SIZE,
				"\r\n----- Entering Guest %d Shell -----\r\n",
				guest_no);

		shell_puts(p_shell, temp_str);
	}

	return 0;
}

int shell_show_cpu_int(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_page();

	if (temp_str == NULL)
		return -ENOMEM;

	get_cpu_interrupt_info(temp_str, CPU_PAGE_SIZE);
	shell_puts(p_shell, temp_str);

	free(temp_str);

	return 0;
}

int shell_show_ptdev_info(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_page();

	if (temp_str == NULL)
		return -ENOMEM;

	get_ptdev_info(temp_str, CPU_PAGE_SIZE);
	shell_puts(p_shell, temp_str);

	free(temp_str);

	return 0;
}

int shell_show_req_info(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_page();

	if (temp_str == NULL)
		return -ENOMEM;

	get_req_info(temp_str, CPU_PAGE_SIZE);
	shell_puts(p_shell, temp_str);

	free(temp_str);

	return 0;
}

int shell_show_vioapic_info(struct shell *p_shell, int argc, char **argv)
{
	char *temp_str = alloc_page();
	uint32_t vmid;

	if (temp_str == NULL)
		return -ENOMEM;

	/* User input invalidation */
	if (argc != 2) {
		snprintf(temp_str, CPU_PAGE_SIZE, "\r\nvmid param needed\r\n");
		goto END;
	} else
		vmid = atoi(argv[1]);

	get_vioapic_info(temp_str, CPU_PAGE_SIZE, vmid);
END:
	shell_puts(p_shell, temp_str);
	free(temp_str);

	return 0;
}

int shell_show_ioapic_info(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_pages(2);

	if (temp_str == NULL)
		return -ENOMEM;

	get_ioapic_info(temp_str, 2 * CPU_PAGE_SIZE);
	shell_puts(p_shell, temp_str);

	free(temp_str);

	return 0;
}

int shell_show_vmexit_profile(struct shell *p_shell,
		__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_pages(2);

	if (temp_str == NULL)
		return -ENOMEM;

	get_vmexit_profile(temp_str, 2*CPU_PAGE_SIZE);
	shell_puts(p_shell, temp_str);

	free(temp_str);

	return 0;
}

int shell_dump_logbuf(__unused struct shell *p_shell,
		int argc, char **argv)
{
	uint32_t pcpu_id;
	int status = -EINVAL;

	if (argc == 2) {
		pcpu_id = atoi(argv[1]);
		print_logmsg_buffer(pcpu_id);
		return 0;
	}

	return status;
}

int shell_get_loglevel(struct shell *p_shell, __unused int argc, __unused char **argv)
{
	char str[MAX_STR_SIZE] = {0};

	snprintf(str, MAX_STR_SIZE,
		"console_loglevel: %u, mem_loglevel: %u\r\n",
		console_loglevel, mem_loglevel);

	shell_puts(p_shell, str);

	return 0;
}

int shell_set_loglevel(struct shell *p_shell, int argc, char **argv)
{
	int status = 0;

	if (argc == 2) {
		console_loglevel = atoi(argv[1]);
	} else if (argc == 3) {
		console_loglevel = atoi(argv[1]);
		mem_loglevel = atoi(argv[2]);
	} else {
		status = -EINVAL;
		shell_puts(p_shell,
			"Please enter correct cmd with "
			"<console_loglevel> [mem_loglevel]\r\n");
	}

	return status;
}

int shell_cpuid(struct shell *p_shell, int argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};
	uint32_t leaf, subleaf = 0;
	uint32_t eax, ebx, ecx, edx;

	if (argc == 2) {
		leaf = strtoul(argv[1], NULL, 16);
	} else if (argc == 3) {
		leaf = strtoul(argv[1], NULL, 16);
		subleaf = strtoul(argv[2], NULL, 16);
	} else {
		shell_puts(p_shell,
			"Please enter correct cmd with "
			"cpuid <leaf> [subleaf]\r\n");
		return -EINVAL;
	}

	cpuid_subleaf(leaf, subleaf, &eax, &ebx, &ecx, &edx);
	snprintf(str, MAX_STR_SIZE,
		"cpuid leaf: 0x%x, subleaf: 0x%x, 0x%x:0x%x:0x%x:0x%x\r\n",
		leaf, subleaf, eax, ebx, ecx, edx);

	shell_puts(p_shell, str);

	return 0;
}

int shell_terminate_serial(struct shell *p_shell)
{
	/* Shell shouldn't own the serial port handle anymore. */
	p_shell->session_io.io_session_info = NULL;

	return 0;
}

void shell_puts_serial(struct shell *p_shell, char *string_ptr)
{
	uint32_t serial_handle =
		(uint32_t)(uint64_t)p_shell->session_io.io_session_info;

	/* Output the string */
	serial_puts(serial_handle, string_ptr,
				strnlen_s(string_ptr, SHELL_STRING_MAX_LEN));
}

uint8_t shell_getc_serial(struct shell *p_shell)
{
	uint32_t serial_handle =
		(uint32_t)(uint64_t)p_shell->session_io.io_session_info;

	return serial_getc(serial_handle);
}

void shell_special_serial(struct shell *p_shell, uint8_t ch)
{
	switch (ch) {
	/* Escape character */
	case 0x1b:
		/* Consume the next 2 characters */
		(void) p_shell->session_io.io_getc(p_shell);
		(void) p_shell->session_io.io_getc(p_shell);
		break;
	default:
		break;
	}
}

int shell_construct(struct shell **p_shell)
{
	int status = 0;
	/* Allocate memory for shell session */
	*p_shell = (struct shell *) calloc(1, sizeof(**p_shell));

	if (!(*p_shell)) {
		pr_err("Error: out of memory");
		status = -ENOMEM;
	}

	return status;
}
