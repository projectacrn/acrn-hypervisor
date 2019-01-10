/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>
#include <ioapic.h>
#include "shell_priv.h"

#define TEMP_STR_SIZE		60U
#define MAX_STR_SIZE		256U
#define SHELL_PROMPT_STR	"ACRN:\\>"

#define SHELL_LOG_BUF_SIZE		(PAGE_SIZE * CONFIG_MAX_PCPU_NUM / 2U)
static char shell_log_buf[SHELL_LOG_BUF_SIZE];

/* Input Line Other - Switch to the "other" input line (there are only two
 * input lines total).
 */
#define SHELL_INPUT_LINE_OTHER(v)	(((v) + 1U) & 0x1U)

static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv);
static int32_t shell_list_vm(__unused int32_t argc, __unused char **argv);
static int32_t shell_list_vcpu(__unused int32_t argc, __unused char **argv);
static int32_t shell_vcpu_dumpreg(int32_t argc, char **argv);
static int32_t shell_dumpmem(int32_t argc, char **argv);
static int32_t shell_to_sos_console(int32_t argc, char **argv);
static int32_t shell_show_cpu_int(__unused int32_t argc, __unused char **argv);
static int32_t shell_show_ptdev_info(__unused int32_t argc, __unused char **argv);
static int32_t shell_show_vioapic_info(int32_t argc, char **argv);
static int32_t shell_show_ioapic_info(__unused int32_t argc, __unused char **argv);
static int32_t shell_loglevel(int32_t argc, char **argv);
static int32_t shell_cpuid(int32_t argc, char **argv);
static int32_t shell_trigger_crash(int32_t argc, char **argv);

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

static void shell_handle_special_char(char ch)
{
	switch (ch) {
	/* Escape character */
	case 0x1b:
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
	(void)strncpy_s(&cmd_argv_str[0], SHELL_CMD_MAX_LEN, p_input_line, SHELL_CMD_MAX_LEN);
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

static int32_t shell_process(void)
{
	int32_t status;
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

	/* Zero fill the input buffer */
	(void)memset((void *)p_shell->input_line[p_shell->input_line_active], 0U,
			SHELL_CMD_MAX_LEN + 1U);
}

#define SHELL_ROWS	10
#define MAX_INDENT_LEN	16U
static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv)
{
	uint16_t spaces;
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

			/* Output the command string */
			shell_puts("  ");
			shell_puts(p_cmd->str);

			/* Calculate spaces needed for alignment */
			spaces = MAX_INDENT_LEN - strnlen_s(p_cmd->str, MAX_INDENT_LEN - 1);

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

static int32_t shell_list_vm(__unused int32_t argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];
	struct acrn_vm *vm;
	uint16_t idx;
	char state[32];

	shell_puts("\r\nVM NAME                  VM ID            VM STATE"
		"\r\n=======                  =====            ========\r\n");

	for (idx = 0U; idx < CONFIG_MAX_VM_NUM; idx++) {
		vm = get_vm_from_vmid(idx);
		if (vm == NULL) {
			continue;
		}
		switch (vm->state) {
		case VM_CREATED:
			(void)strncpy_s(state, 32U, "Created", 32U);
			break;
		case VM_STARTED:
			(void)strncpy_s(state, 32U, "Started", 32U);
			break;
		case VM_PAUSED:
			(void)strncpy_s(state, 32U, "Paused", 32U);
			break;
		default:
			(void)strncpy_s(state, 32U, "Unknown", 32U);
			break;
		}
		/* Create output string consisting of VM name and VM id
		 */
		snprintf(temp_str, MAX_STR_SIZE,
				"vm_%-24d %-16d %-8s\r\n", vm->vm_id,
				vm->vm_id, state);

		/* Output information for this task */
		shell_puts(temp_str);
	}

	return 0;
}

static int32_t shell_list_vcpu(__unused int32_t argc, __unused char **argv)
{
	char temp_str[MAX_STR_SIZE];
	struct acrn_vm *vm;
	struct acrn_vcpu *vcpu;
	char state[32];
	uint16_t i;
	uint16_t idx;

	shell_puts("\r\nVM ID    PCPU ID    VCPU ID    VCPU ROLE    VCPU STATE"
		"\r\n=====    =======    =======    =========    ==========\r\n");

	for (idx = 0U; idx < CONFIG_MAX_VM_NUM; idx++) {
		vm = get_vm_from_vmid(idx);
		if (vm == NULL) {
			continue;
		}
		foreach_vcpu(i, vm, vcpu) {
			switch (vcpu->state) {
			case VCPU_INIT:
				(void)strncpy_s(state, 32U, "Init", 32U);
				break;
			case VCPU_PAUSED:
				(void)strncpy_s(state, 32U, "Paused", 32U);
				break;
			case VCPU_RUNNING:
				(void)strncpy_s(state, 32U, "Running", 32U);
				break;
			case VCPU_ZOMBIE:
				(void)strncpy_s(state, 32U, "Zombie", 32U);
				break;
			default:
				(void)strncpy_s(state, 32U, "Unknown", 32U);
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

	return 0;
}

#define DUMPREG_SP_SIZE	32
/* the input 'data' must != NULL and indicate a vcpu structure pointer */
static void vcpu_dumpreg(void *data)
{
	int32_t status;
	uint64_t i, fault_addr, tmp[DUMPREG_SP_SIZE];
	uint32_t err_code = 0;
	struct vcpu_dump *dump = data;
	struct acrn_vcpu *vcpu = dump->vcpu;
	char *str = dump->str;
	size_t len, size = dump->str_max;

	len = snprintf(str, size,
		"=  VM ID %d ==== CPU ID %hu========================\r\n"
		"=  RIP=0x%016llx  RSP=0x%016llx RFLAGS=0x%016llx\r\n"
		"=  CR0=0x%016llx  CR2=0x%016llx\r\n"
		"=  CR3=0x%016llx  CR4=0x%016llx\r\n"
		"=  RAX=0x%016llx  RBX=0x%016llx RCX=0x%016llx\r\n"
		"=  RDX=0x%016llx  RDI=0x%016llx RSI=0x%016llx\r\n"
		"=  RBP=0x%016llx  R8=0x%016llx R9=0x%016llx\r\n"
		"=  R10=0x%016llx  R11=0x%016llx R12=0x%016llx\r\n"
		"=  R13=0x%016llx  R14=0x%016llx  R15=0x%016llx\r\n",
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
		len = snprintf(str, size, "\r\nDump RSP for vm %hu, from gva 0x%016llx\r\n",
			vcpu->vm->vm_id, vcpu_get_gpreg(vcpu, CPU_REG_RSP));
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;

		for (i = 0UL; i < 8UL; i++) {
			len = snprintf(str, size, "=  0x%016llx  0x%016llx 0x%016llx  0x%016llx\r\n",
					tmp[i*4UL], tmp[(i*4UL)+1UL], tmp[(i*4UL)+2UL], tmp[(i*4UL)+3UL]);
			if (len >= size) {
				goto overflow;
			}
			size -= len;
			str += len;
		}
	}
	return;

overflow:
	printf("buffer size could not be enough! please check!\n");
}

static int32_t shell_vcpu_dumpreg(int32_t argc, char **argv)
{
	int32_t status = 0;
	uint16_t vm_id;
	uint16_t vcpu_id;
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
	vm_id = (uint16_t)status;
	vcpu_id = (uint16_t)strtol_deci(argv[2]);

	vm = get_vm_from_vmid(vm_id);
	if (vm == NULL) {
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
	dump.vcpu = vcpu;
	dump.str = shell_log_buf;
	dump.str_max = SHELL_LOG_BUF_SIZE;
	if (vcpu->pcpu_id == get_cpu_id()) {
		vcpu_dumpreg(&dump);
	} else {
		bitmap_set_nolock(vcpu->pcpu_id, &mask);
		smp_call_function(mask, vcpu_dumpreg, &dump);
	}
	shell_puts(shell_log_buf);
	status = 0;

out:
	return status;
}

#define MAX_MEMDUMP_LEN		(32U * 8U)
static int32_t shell_dumpmem(int32_t argc, char **argv)
{
	uint64_t addr;
	uint64_t *ptr;
	uint32_t i, length;
	char temp_str[MAX_STR_SIZE];

	/* User input invalidation */
	if ((argc != 2) && (argc != 3)) {
		return -EINVAL;
	}

	addr = strtoul_hex(argv[1]);
	length = (uint32_t)strtol_deci(argv[2]);
	if (length > MAX_MEMDUMP_LEN) {
		shell_puts("over max length, round back\r\n");
		length = MAX_MEMDUMP_LEN;
	}

	snprintf(temp_str, MAX_STR_SIZE,
		"Dump physical memory addr: 0x%016llx, length %d:\r\n",
		addr, length);
	shell_puts(temp_str);

	ptr = (uint64_t *)addr;
	for (i = 0U; i < (length >> 5U); i++) {
		snprintf(temp_str, MAX_STR_SIZE,
			"=  0x%016llx  0x%016llx  0x%016llx  0x%016llx\r\n",
			*(ptr + (i * 4U)), *(ptr + ((i * 4U) + 1U)),
			*(ptr + ((i * 4U) + 2U)), *(ptr + ((i * 4U) + 3U)));
		shell_puts(temp_str);
	}

	if ((length & 0x1fU) != 0U) {
		snprintf(temp_str, MAX_STR_SIZE,
			"=  0x%016llx  0x%016llx  0x%016llx 0x%016llx\r\n",
			*(ptr + (i * 4U)), *(ptr + ((i * 4U) + 1U)),
			*(ptr + ((i * 4U) + 2U)), *(ptr + ((i * 4U) + 3U)));
		shell_puts(temp_str);
	}

	return 0;
}

static int32_t shell_to_sos_console(__unused int32_t argc, __unused char **argv)
{
	char temp_str[TEMP_STR_SIZE];
	uint16_t guest_no = 0U;

	struct acrn_vm *vm;
	struct acrn_vuart *vu;
#ifdef CONFIG_PARTITION_MODE
	struct vm_description *vm_desc;

	if (argc == 2U) {
		guest_no = strtol_deci(argv[1]);
	}

	vuart_vmid = guest_no;
#endif
	/* Get the virtual device node */
	vm = get_vm_from_vmid(guest_no);
	if (vm == NULL) {
		return -EINVAL;
	}

#ifdef CONFIG_PARTITION_MODE
	vm_desc = vm->vm_desc;
	if (vm_desc != NULL && vm_desc->vm_vuart == false) {
		snprintf(temp_str, TEMP_STR_SIZE, "No vUART configured for vm%d\n", guest_no);
		shell_puts(temp_str);
		return 0;
	}
#endif

	vu = vm_vuart(vm);
	/* UART is now owned by the SOS.
	 * Indicate by toggling the flag.
	 */
	vu->active = true;
	/* Output that switching to SOS shell */
	snprintf(temp_str, TEMP_STR_SIZE,
			"\r\n----- Entering Guest %d Shell -----\r\n",
			guest_no);

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
		uint32_t *pin, uint32_t *vpin, uint32_t *bdf, uint32_t *vbdf)
{
	if (is_entry_active(entry)) {
		if (entry->intr_type == PTDEV_INTR_MSI) {
			(void)strncpy_s(type, 16U, "MSI", 16U);
			*dest = (entry->msi.pmsi_addr & 0xFF000U) >> PAGE_SHIFT;
			if ((entry->msi.pmsi_data & APIC_TRIGMOD_LEVEL) != 0U) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pin = INVALID_INTERRUPT_PIN;
			*vpin = INVALID_INTERRUPT_PIN;
			*bdf = entry->phys_sid.msi_id.bdf;
			*vbdf = entry->virt_sid.msi_id.bdf;
		} else {
			uint32_t phys_irq = entry->allocated_pirq;
			union ioapic_rte rte;

			if (entry->virt_sid.intx_id.src == PTDEV_VPIN_IOAPIC) {
				(void)strncpy_s(type, 16U, "IOAPIC", 16U);
			} else {
				(void)strncpy_s(type, 16U, "PIC", 16U);
			}
			ioapic_get_rte(phys_irq, &rte);
			*dest = rte.full >> IOAPIC_RTE_DEST_SHIFT;
			if ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL) {
				*lvl_tm = true;
			} else {
				*lvl_tm = false;
			}
			*pin = entry->phys_sid.intx_id.pin;
			*vpin = entry->virt_sid.intx_id.pin;
			*bdf = 0U;
			*vbdf = 0U;
		}
		*irq = entry->allocated_pirq;
		*vector = irq_to_vector(entry->allocated_pirq);
	} else {
		(void)strncpy_s(type, 16U, "NONE", 16U);
		*irq = IRQ_INVALID;
		*vector = 0U;
		*dest = 0UL;
		*lvl_tm = 0;
		*pin = -1;
		*vpin = -1;
		*bdf = 0U;
		*vbdf = 0U;
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
	uint32_t pin, vpin;
	uint32_t bdf, vbdf;

	len = snprintf(str, size, "\r\nVM\tTYPE\tIRQ\tVEC\tDEST\tTM\tPIN\tVPIN\tBDF\tVBDF");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	for (idx = 0U; idx < CONFIG_MAX_PT_IRQ_ENTRIES; idx++) {
		entry = &ptirq_entries[idx];
		if (is_entry_active(entry)) {
			get_entry_info(entry, type, &irq, &vector,
					&dest, &lvl_tm, &pin, &vpin,
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
					pin, vpin, (bdf & 0xff00U) >> 8U,
					(bdf & 0xf8U) >> 3U, bdf & 0x7U,
					(vbdf & 0xff00U) >> 8U,
					(vbdf & 0xf8U) >> 3U, vbdf & 0x7U);
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
	uint32_t pin, pincount;

	if (vm == NULL) {
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

	pincount = vioapic_pincount(vm);
	rte.full = 0UL;
	for (pin = 0U; pin < pincount; pin++) {
		vioapic_get_rte(vm, pin, &rte);
		mask = ((rte.full & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
		remote_irr = ((rte.full & IOAPIC_RTE_REM_IRR) == IOAPIC_RTE_REM_IRR);
		phys = ((rte.full & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
		delmode = (uint32_t)(rte.full & IOAPIC_RTE_DELMOD);
		level = ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL);
		vector = rte.u.lo_32 & IOAPIC_RTE_LOW_INTVEC;
		dest = (uint32_t)(rte.full >> IOAPIC_RTE_DEST_SHIFT);

		len = snprintf(str, size, "\r\n%hhu\t0x%X\t%s\t0x%X\t%s\t%u\t%d\t%d",
				pin, vector, phys ? "phys" : "logic", dest, level ? "level" : "edge",
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
		vmid = (uint16_t) ret;
		get_vioapic_info(shell_log_buf, SHELL_LOG_BUF_SIZE, vmid);
		shell_puts(shell_log_buf);
		return 0;
	}

	return -EINVAL;
}

static void get_rte_info(union ioapic_rte rte, bool *mask, bool *irr,
	bool *phys, uint32_t *delmode, bool *level, uint32_t *vector, uint32_t *dest)
{
	*mask = ((rte.full & IOAPIC_RTE_INTMASK) == IOAPIC_RTE_INTMSET);
	*irr = ((rte.full & IOAPIC_RTE_REM_IRR) == IOAPIC_RTE_REM_IRR);
	*phys = ((rte.full & IOAPIC_RTE_DESTMOD) == IOAPIC_RTE_DESTPHY);
	*delmode = (uint32_t)(rte.full & IOAPIC_RTE_DELMOD);
	*level = ((rte.full & IOAPIC_RTE_TRGRLVL) != 0UL);
	*vector = (uint32_t)(rte.full & IOAPIC_RTE_INTVEC);
	*dest = (uint32_t)(rte.full >> APIC_ID_SHIFT);
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
	uint32_t irq;
	size_t len, size = str_max_len;
	uint32_t ioapic_nr_gsi = 0U;

	len = snprintf(str, size, "\r\nIRQ\tPIN\tRTE.HI32\tRTE.LO32\tVEC\tDST\tDM\tTM\tDELM\tIRR\tMASK");
	if (len >= size) {
		goto overflow;
	}
	size -= len;
	str += len;

	ioapic_nr_gsi = ioapic_get_nr_gsi ();
	for (irq = 0U; irq < ioapic_nr_gsi; irq++) {
		void *addr = ioapic_get_gsi_irq_addr(irq);
		uint32_t pin = ioapic_irq_to_pin(irq);
		union ioapic_rte rte;

		bool irr, phys, level, mask;
		uint32_t delmode, vector, dest;

		/* Add NULL check for addr, INVALID_PIN check for pin */
		if ((addr == NULL) || (!ioapic_is_pin_valid(pin))) {
			goto overflow;
		}

		ioapic_get_rte_entry(addr, pin, &rte);

		get_rte_info(rte, &mask, &irr, &phys, &delmode, &level, &vector, &dest);

		len = snprintf(str, size, "\r\n%03d\t%03hhu\t0x%08X\t0x%08X\t", irq, pin, rte.u.hi_32, rte.u.lo_32);
		if (len >= size) {
			goto overflow;
		}
		size -= len;
		str += len;

		len = snprintf(str, size, "0x%02X\t0x%02X\t%s\t%s\t%u\t%d\t%d",
			vector, dest, phys ? "phys" : "logic", level ? "level" : "edge", delmode >> 8, irr, mask);
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

static int32_t shell_trigger_crash(int32_t argc, char **argv)
{
	char str[MAX_STR_SIZE] = {0};

	(void)argc;
	(void)argv;
	snprintf(str, MAX_STR_SIZE, "trigger crash, divide by 0 ...\r\n");
	shell_puts(str);

	asm("movl $0x1, %eax");
	asm("movl $0x0, %ecx");
	asm("idiv  %ecx");

	return 0;
}
