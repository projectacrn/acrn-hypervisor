/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include "shell_priv.h"

#define TEMP_STR_SIZE		60U
#define MAX_STR_SIZE		256U
#define SHELL_PROMPT_STR	"ACRN:\\>"

/* Input Line Other - Switch to the "other" input line (there are only two
 * input lines total).
 */
#define SHELL_INPUT_LINE_OTHER(v)	(((v) + 1U) % 2U)

static struct shell_cmd shell_cmds[] = {
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
		.str		= SHELL_CMD_VCPU_DUMPREG,
		.cmd_param	= SHELL_CMD_VCPU_DUMPREG_PARAM,
		.help_str	= SHELL_CMD_VCPU_DUMPREG_HELP,
		.fcn		= shell_vcpu_dumpreg,
	},
	{
		.str		= SHELL_CMD_DUMPMEM,
		.cmd_param	= SHELL_CMD_DUMPMEM_PARAM,
		.help_str	= SHELL_CMD_DUMPMEM_HELP,
		.fcn		= shell_dumpmem,
	},
	{
		.str		= SHELL_CMD_SOS_CONSOLE,
		.cmd_param	= SHELL_CMD_SOS_CONSOLE_PARAM,
		.help_str	= SHELL_CMD_SOS_CONSOLE_HELP,
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
		.fcn		= shell_trigger_crash,
	},
};

/* The initial log level*/
uint32_t console_loglevel = CONFIG_CONSOLE_LOGLEVEL_DEFAULT;
uint32_t mem_loglevel = CONFIG_MEM_LOGLEVEL_DEFAULT;

static struct shell hv_shell;
static struct shell *p_shell = &hv_shell;

static int string_to_argv(char *argv_str, void *p_argv_mem,
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

static char shell_getc(void)
{
	return console_getc();
}

static void shell_handle_special_char(uint8_t ch)
{
	switch (ch) {
	/* Escape character */
	case 0x1bU:
		/* Consume the next 2 characters */
		(void) shell_getc();
		(void) shell_getc();
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

static bool shell_input_line(void)
{
	bool done = false;
	char ch;

	ch = shell_getc();

	/* Check character */
	switch (ch) {
	/* Backspace */
	case '\b':
		/* Ensure length is not 0 */
		if (p_shell->input_line_len > 0U) {
			/* Reduce the length of the string by one */
			p_shell->input_line_len--;

			/* Null terminate the last character to erase it */
			p_shell->input_line[p_shell->input_line_active]
					[p_shell->input_line_len] = 0;

			/* Echo backspace */
			shell_puts("\b");

			/* Send a space + backspace sequence to delete
			 * character
			 */
			shell_puts(" \b");
		}
		break;

	/* Carriage-return */
	case '\r':
		/* Echo carriage return / line feed */
		shell_puts("\r\n");

		/* Set flag showing line input done */
		done = true;

		/* Reset command length for next command processing */
		p_shell->input_line_len = 0U;
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
				/* Echo back the input */
				shell_puts(&p_shell->input_line
						[p_shell->input_line_active]
						[p_shell->input_line_len]);

				/* Move to next character in string */
				p_shell->input_line_len++;
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

		}
		break;
	}


	return done;
}

static int shell_process(void)
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
	status = shell_process_cmd(p_input_line);

	/* Now that the command is processed, zero fill the input buffer */
	(void)memset((void *) p_shell->input_line[p_shell->input_line_active],
			0, SHELL_CMD_MAX_LEN + 1U);

	/* Process command and return result to caller */
	return status;
}

struct shell_cmd *shell_find_cmd(const char *cmd_str)
{
	uint32_t i;
	struct shell_cmd *p_cmd = NULL;

	for (i = 0U; i < p_shell->cmd_count; i++) {
		p_cmd = &p_shell->shell_cmd[i];
		if (strcmp(p_cmd->str, cmd_str) == 0) {
			return p_cmd;
		}
	}
	return NULL;
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

int shell_process_cmd(char *p_input_line)
{
	int status = -EINVAL;
	struct shell_cmd *p_cmd;
	char cmd_argv_str[SHELL_CMD_MAX_LEN + 1U];
	int cmd_argv_mem[sizeof(char *) * ((SHELL_CMD_MAX_LEN + 1U) / 2U)];
	int cmd_argc;
	char **cmd_argv;

	/* Copy the input line INTo an argument string to become part of the
	 * argument vector.
	 */
	(void)strcpy_s(&cmd_argv_str[0], SHELL_CMD_MAX_LEN, p_input_line);
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
		}
	}

	return status;
}

void shell_init(void)
{
	p_shell->shell_cmd = shell_cmds;
	p_shell->cmd_count = ARRAY_SIZE(shell_cmds);

	(void)strcpy_s((void *)p_shell->name, SHELL_NAME_MAX_LEN, "Serial");

	/* Zero fill the input buffer */
	(void)memset((void *)p_shell->input_line[p_shell->input_line_active], 0U,
			SHELL_CMD_MAX_LEN + 1U);
}

#define SHELL_ROWS	10
#define MAX_INDENT_LEN	16
int shell_cmd_help(__unused int argc, __unused char **argv)
{
	int spaces = 0;
	struct shell_cmd *p_cmd = NULL;
	char space_buf[MAX_INDENT_LEN + 1];

	/* Print title */
	shell_puts("\r\nRegistered Commands:\r\n\r\n");

	pr_dbg("shell: Number of registered commands = %u in %s\n",
		p_shell->cmd_count, __func__);

	(void)memset(space_buf, ' ', sizeof(space_buf));
	/* Proceed based on the number of registered commands. */
	if (p_shell->cmd_count == 0U) {
		/* No registered commands */
		shell_puts("NONE\r\n");
	} else {
		int32_t i = 0;
		uint32_t j;

		for (j = 0U; j < p_shell->cmd_count; j++) {
			p_cmd = &p_shell->shell_cmd[j];

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

			/* Output the command string */
			shell_puts("  ");
			shell_puts(p_cmd->str);

			/* Calculate spaces needed for alignment */
			spaces = MAX_INDENT_LEN - strnlen_s(p_cmd->str,
					MAX_INDENT_LEN - 1);

			space_buf[spaces] = '\0';
			shell_puts(space_buf);
			space_buf[spaces] = ' ';

			/* Display parameter info if applicable. */
			if (p_cmd->cmd_param != NULL) {
				shell_puts(p_cmd->cmd_param);
			}

			/* Display help text if available. */
			if (p_cmd->help_str != NULL) {
				shell_puts(" - ");
				shell_puts(p_cmd->help_str);
			}
			shell_puts("\r\n");
		}
	}

	shell_puts("\r\n");

	return 0;
}

int shell_list_vm(__unused int argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];
	struct list_head *pos;
	struct vm *vm;

	shell_puts("\r\nVM NAME                  VM ID            VM STATE"
		"\r\n=======                  =====            ========\r\n");

	spinlock_obtain(&vm_list_lock);
	list_for_each(pos, &vm_list) {
		char state[32];

		vm = list_entry(pos, struct vm, list);
		switch (vm->state) {
		case VM_CREATED:
			(void)strcpy_s(state, 32, "Created"); break;
		case VM_STARTED:
			(void)strcpy_s(state, 32, "Started"); break;
		case VM_PAUSED:
			(void)strcpy_s(state, 32, "Paused"); break;
		default:
			(void)strcpy_s(state, 32, "Unknown"); break;
		}
		/* Create output string consisting of VM name and VM id
		 */
		snprintf(temp_str, MAX_STR_SIZE,
				"vm_%-24d %-16d %-8s\r\n", vm->vm_id,
				vm->vm_id, state);

		/* Output information for this task */
		shell_puts(temp_str);
	}
	spinlock_release(&vm_list_lock);

	return 0;
}

int shell_list_vcpu(__unused int argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];
	struct list_head *pos;
	struct vm *vm;
	struct vcpu *vcpu;

	shell_puts("\r\nVM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE"
		"\r\n=====    =======    =======    =========    ==========\r\n");

	spinlock_obtain(&vm_list_lock);
	list_for_each(pos, &vm_list) {
		char state[32];
		uint16_t i;

		vm = list_entry(pos, struct vm, list);
		foreach_vcpu(i, vm, vcpu) {
			switch (vcpu->state) {
			case VCPU_INIT:
				(void)strcpy_s(state, 32, "Init"); break;
			case VCPU_PAUSED:
				(void)strcpy_s(state, 32, "Paused"); break;
			case VCPU_RUNNING:
				(void)strcpy_s(state, 32, "Running"); break;
			case VCPU_ZOMBIE:
				(void)strcpy_s(state, 32, "Zombie"); break;
			default:
				(void)strcpy_s(state, 32, "Unknown");
			}
			/* Create output string consisting of VM name
			 * and VM id
			 */
			snprintf(temp_str, MAX_STR_SIZE,
					"  %-9d %-10d %-7hu %-12s %-16s\r\n",
					vm->vm_id,
					vcpu->pcpu_id,
					vcpu->vcpu_id,
					is_vcpu_bsp(vcpu) ?
					"PRIMARY" : "SECONDARY",
					state);
			/* Output information for this task */
			shell_puts(temp_str);
		}
	}
	spinlock_release(&vm_list_lock);

	return 0;
}

#define DUMPREG_SP_SIZE	32
int shell_vcpu_dumpreg(int argc, char **argv)
{
	int status = 0;
	uint16_t vm_id;
	uint16_t vcpu_id;
	char temp_str[MAX_STR_SIZE];
	struct vm *vm;
	struct vcpu *vcpu;
	uint64_t i, fault_addr;
	uint64_t tmp[DUMPREG_SP_SIZE];
	uint32_t err_code = 0;

	/* User input invalidation */
	if (argc != 3) {
		shell_puts("Please enter cmd with <vm_id, vcpu_id>\r\n");
		return -EINVAL;
	}

	status = atoi(argv[1]);
	if (status < 0) {
		return -EINVAL;
	}
	vm_id = (uint16_t)status;
	vcpu_id = (uint16_t)atoi(argv[2]);
	if (vcpu_id >= phys_cpu_num) {
		return (-EINVAL);
	}
	vm = get_vm_from_vmid(vm_id);
	if (vm == NULL) {
		shell_puts("No vm found in the input <vm_id, vcpu_id>\r\n");
		return -EINVAL;
	}

	vcpu = vcpu_from_vid(vm, vcpu_id);
	if (vcpu == NULL) {
		shell_puts("No vcpu found in the input <vm_id, vcpu_id>\r\n");
		return -EINVAL;
	}

	if (vcpu->state != VCPU_PAUSED) {
		shell_puts("NOTE: VCPU unPAUSEed, regdump "
				"may not be accurate\r\n");
	}

	snprintf(temp_str, MAX_STR_SIZE,
		"=  VM ID %d ==== CPU ID %hu========================\r\n",
		vm->vm_id, vcpu->vcpu_id);
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RIP=0x%016llx  RSP=0x%016llx "
			"RFLAGS=0x%016llx\r\n", vcpu_get_rip(vcpu),
			vcpu_get_gpreg(vcpu, CPU_REG_RSP),
			vcpu_get_rflags(vcpu));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  CR0=0x%016llx  CR2=0x%016llx\r\n",
			vcpu_get_cr0(vcpu), vcpu_get_cr2(vcpu));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  CR3=0x%016llx  CR4=0x%016llx\r\n",
			exec_vmread(VMX_GUEST_CR3), vcpu_get_cr4(vcpu));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RAX=0x%016llx  RBX=0x%016llx  "
			"RCX=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RAX),
			vcpu_get_gpreg(vcpu, CPU_REG_RBX),
			vcpu_get_gpreg(vcpu, CPU_REG_RCX));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RDX=0x%016llx  RDI=0x%016llx  "
			"RSI=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RDX),
			vcpu_get_gpreg(vcpu, CPU_REG_RDI),
			vcpu_get_gpreg(vcpu, CPU_REG_RSI));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  RBP=0x%016llx  R8=0x%016llx  "
			"R9=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_RBP),
			vcpu_get_gpreg(vcpu, CPU_REG_R8),
			vcpu_get_gpreg(vcpu, CPU_REG_R9));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE, "=  R10=0x%016llx  R11=0x%016llx  "
			"R12=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_R10),
			vcpu_get_gpreg(vcpu, CPU_REG_R11),
			vcpu_get_gpreg(vcpu, CPU_REG_R12));
	shell_puts(temp_str);
	snprintf(temp_str, MAX_STR_SIZE,
			"=  R13=0x%016llx  R14=0x%016llx  R15=0x%016llx\r\n",
			vcpu_get_gpreg(vcpu, CPU_REG_R13),
			vcpu_get_gpreg(vcpu, CPU_REG_R14),
			vcpu_get_gpreg(vcpu, CPU_REG_R15));
	shell_puts(temp_str);

	/* dump sp */
	status = copy_from_gva(vcpu, tmp, vcpu_get_gpreg(vcpu, CPU_REG_RSP),
			DUMPREG_SP_SIZE*sizeof(uint64_t), &err_code,
			&fault_addr);
	if (status < 0) {
		/* copy_from_gva fail */
		shell_puts("Cannot handle user gva yet!\r\n");
	} else {
		snprintf(temp_str, MAX_STR_SIZE,
				"\r\nDump RSP for vm %hu, from "
				"gva 0x%016llx\r\n",
				vm_id, vcpu_get_gpreg(vcpu, CPU_REG_RSP));
		shell_puts(temp_str);

		for (i = 0UL; i < 8UL; i++) {
			snprintf(temp_str, MAX_STR_SIZE,
					"=  0x%016llx  0x%016llx  "
					"0x%016llx  0x%016llx\r\n",
					tmp[i*4UL], tmp[(i*4UL)+1UL],
					tmp[(i*4UL)+2UL], tmp[(i*4UL)+3UL]);
			shell_puts(temp_str);
		}
	}

	return status;
}

#define MAX_MEMDUMP_LEN		(32U*8U)
int shell_dumpmem(int argc, char **argv)
{
	uint64_t addr;
	uint64_t *ptr;
	uint32_t i, length = 32U;
	char temp_str[MAX_STR_SIZE];

	/* User input invalidation */
	if (argc != 2 && argc != 3) {
		return -EINVAL;
	}

	addr = strtoul_hex(argv[1]);
	length = atoi(argv[2]);
	if (length > MAX_MEMDUMP_LEN) {
		shell_puts("over max length, round back\r\n");
		length = MAX_MEMDUMP_LEN;
	}

	snprintf(temp_str, MAX_STR_SIZE,
		"Dump physical memory addr: 0x%016llx, length %d:\r\n",
		addr, length);
	shell_puts(temp_str);

	ptr = (uint64_t *)addr;
	for (i = 0U; i < (length/32U); i++) {
		snprintf(temp_str, MAX_STR_SIZE,
			"=  0x%016llx  0x%016llx  0x%016llx  0x%016llx\r\n",
			*(ptr + (i*4)), *(ptr + ((i*4)+1)),
			*(ptr + ((i*4)+2)), *(ptr + ((i*4)+3)));
		shell_puts(temp_str);
	}

	if ((length % 32U) != 0) {
		snprintf(temp_str, MAX_STR_SIZE,
			"=  0x%016llx  0x%016llx  0x%016llx 0x%016llx\r\n",
			*(ptr + (i*4)), *(ptr + ((i*4)+1)),
			*(ptr + ((i*4)+2)), *(ptr + ((i*4)+3)));
		shell_puts(temp_str);
	}

	return 0;
}

int shell_to_sos_console(__unused int argc, __unused char **argv)
{
	char temp_str[TEMP_STR_SIZE];
	uint16_t guest_no = 0U;

	struct vm *vm;
	struct vuart *vuart;

	/* Get the virtual device node */
	vm = get_vm_from_vmid(guest_no);
	if (vm == NULL) {
		return -EINVAL;
	}
	vuart = vm->vuart;
	if (vuart == NULL) {
		snprintf(temp_str, TEMP_STR_SIZE,
				"\r\nError: serial console driver is not "
				"enabled for VM %d\r\n",
				guest_no);
		shell_puts(temp_str);
	} else {
		/* UART is now owned by the SOS.
		 * Indicate by toggling the flag.
		 */
		vuart->active = true;
		/* Output that switching to SOS shell */
		snprintf(temp_str, TEMP_STR_SIZE,
				"\r\n----- Entering Guest %d Shell -----\r\n",
				guest_no);

		shell_puts(temp_str);
	}

	return 0;
}

int shell_show_cpu_int(__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_page();

	if (temp_str == NULL) {
		return -ENOMEM;
	}

	get_cpu_interrupt_info(temp_str, CPU_PAGE_SIZE);
	shell_puts(temp_str);

	free(temp_str);

	return 0;
}

int shell_show_ptdev_info(__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_page();

	if (temp_str == NULL) {
		return -ENOMEM;
	}

	get_ptdev_info(temp_str, CPU_PAGE_SIZE);
	shell_puts(temp_str);

	free(temp_str);

	return 0;
}

int shell_show_vioapic_info(int argc, char **argv)
{
	char *temp_str = alloc_page();
	uint16_t vmid;
	int32_t ret;

	if (temp_str == NULL) {
		return -ENOMEM;
	}

	/* User input invalidation */
	if (argc != 2) {
		free(temp_str);
		return -EINVAL;
	}
	ret = atoi(argv[1]);
	if (ret >= 0) {
		vmid = (uint16_t) ret;
		get_vioapic_info(temp_str, CPU_PAGE_SIZE, vmid);
		shell_puts(temp_str);
		free(temp_str);
		return 0;
	}

	free(temp_str);
	return -EINVAL;
}

int shell_show_ioapic_info(__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_pages(2U);

	if (temp_str == NULL) {
		return -ENOMEM;
	}

	get_ioapic_info(temp_str, 2 * CPU_PAGE_SIZE);
	shell_puts(temp_str);

	free(temp_str);

	return 0;
}

int shell_show_vmexit_profile(__unused int argc, __unused char **argv)
{
	char *temp_str = alloc_pages(2U);

	if (temp_str == NULL) {
		return -ENOMEM;
	}

	get_vmexit_profile(temp_str, 2*CPU_PAGE_SIZE);
	shell_puts(temp_str);

	free(temp_str);

	return 0;
}

int shell_dump_logbuf(int argc, char **argv)
{
	uint16_t pcpu_id;
	int val;

	if (argc == 2) {
		val = atoi(argv[1]);
		if (val >= 0) {
			pcpu_id = (uint16_t)val;
			print_logmsg_buffer(pcpu_id);
			return 0;
		}
	}

	return -EINVAL;
}

int shell_loglevel(int argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};

	if (argc == 1) {
		snprintf(str, MAX_STR_SIZE,
			"console_loglevel: %u, mem_loglevel: %u\r\n",
			console_loglevel, mem_loglevel);
		shell_puts(str);
	} else if (argc == 2) {
		console_loglevel = atoi(argv[1]);
	} else if (argc == 3) {
		console_loglevel = atoi(argv[1]);
		mem_loglevel = atoi(argv[2]);
	} else {
		return -EINVAL;
	}

	return 0;
}

int shell_cpuid(int argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};
	uint32_t leaf, subleaf = 0;
	uint32_t eax, ebx, ecx, edx;

	if (argc == 2) {
		leaf = strtoul_hex(argv[1]);
	} else if (argc == 3) {
		leaf = strtoul_hex(argv[1]);
		subleaf = strtoul_hex(argv[2]);
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

int shell_trigger_crash(int argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};

	(void)argc;
	(void)argv;
	snprintf(str, MAX_STR_SIZE, "trigger crash, divide by 0 ...\r\n");
	shell_puts(str);
	snprintf(str, MAX_STR_SIZE, "%d\r", 1/0);

	return 0;
}

void shell_puts(const char *string_ptr)
{
	/* Output the string */
	(void)console_write(string_ptr, strnlen_s(string_ptr,
				SHELL_STRING_MAX_LEN));
}


