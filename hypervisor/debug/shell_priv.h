/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHELL_PRIV_H
#define SHELL_PRIV_H

#include <x86/lib/spinlock.h>

#define SHELL_CMD_MAX_LEN		100U
#define SHELL_STRING_MAX_LEN		(PAGE_SIZE << 2U)

extern int16_t console_vmid;

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
#define SHELL_CMD_HELP_HELP		"Display information about supported hypervisor shell commands"

#define SHELL_CMD_VERSION		"version"
#define SHELL_CMD_VERSION_PARAM		NULL
#define SHELL_CMD_VERSION_HELP		"Display the HV version information"

#define SHELL_CMD_VM_LIST		"vm_list"
#define SHELL_CMD_VM_LIST_PARAM		NULL
#define SHELL_CMD_VM_LIST_HELP		"List all VMs, displaying the VM UUID, ID, name and state"

#define SHELL_CMD_VCPU_LIST		"vcpu_list"
#define SHELL_CMD_VCPU_LIST_PARAM	NULL
#define SHELL_CMD_VCPU_LIST_HELP	"List all vCPUs in all VMs"

#define SHELL_CMD_VCPU_DUMPREG		"vcpu_dumpreg"
#define SHELL_CMD_VCPU_DUMPREG_PARAM	"<vm id, vcpu id>"
#define SHELL_CMD_VCPU_DUMPREG_HELP	"Dump registers for a specific vCPU"

#define SHELL_CMD_DUMP_HOST_MEM		"dump_host_mem"
#define SHELL_CMD_DUMP_HOST_MEM_PARAM	"<addr, length>"
#define SHELL_CMD_DUMP_HOST_MEM_HELP	"Dump host memory, starting at a given address(Hex), and for a given length (Dec in bytes)"

#define SHELL_CMD_DUMP_GUEST_MEM	"dump_guest_mem"
#define SHELL_CMD_DUMP_GUEST_MEM_PARAM	"<vm_id, addr, length>"
#define SHELL_CMD_DUMP_GUEST_MEM_HELP	"Dump guest memory, vm id(Dec), starting at a given address(Hex), and for a given length (Dec in bytes)"

#define SHELL_CMD_VM_CONSOLE		"vm_console"
#define SHELL_CMD_VM_CONSOLE_PARAM	"<vm id>"
#define SHELL_CMD_VM_CONSOLE_HELP	"Switch to the VM's console. Use [Ctrl+Spacebar] to return to the ACRN shell "\
					"console"

#define SHELL_CMD_INTERRUPT		"int"
#define SHELL_CMD_INTERRUPT_PARAM	NULL
#define SHELL_CMD_INTERRUPT_HELP	"List interrupt information per CPU"

#define SHELL_CMD_PTDEV			"pt"
#define SHELL_CMD_PTDEV_PARAM		NULL
#define SHELL_CMD_PTDEV_HELP		"Show pass-through device information"

#define SHELL_CMD_REBOOT		"reboot"
#define SHELL_CMD_REBOOT_PARAM		NULL
#define SHELL_CMD_REBOOT_HELP		"Trigger a system reboot (immediately)"

#define SHELL_CMD_IOAPIC		"dump_ioapic"
#define SHELL_CMD_IOAPIC_PARAM		NULL
#define SHELL_CMD_IOAPIC_HELP		"Show native IOAPIC information"

#define SHELL_CMD_VIOAPIC		"vioapic"
#define SHELL_CMD_VIOAPIC_PARAM		"<vm id>"
#define SHELL_CMD_VIOAPIC_HELP		"Show virtual IOAPIC (vIOAPIC) information for a specific VM"

#define SHELL_CMD_LOG_LVL		"loglevel"
#define SHELL_CMD_LOG_LVL_PARAM		"[<console_loglevel> [<mem_loglevel> [npk_loglevel]]]"
#define SHELL_CMD_LOG_LVL_HELP		"No argument: get the level of logging for the console, memory and npk. Set "\
					"the level by giving (up to) 3 parameters between 0 and 6 (verbose)"

#define SHELL_CMD_CPUID			"cpuid"
#define SHELL_CMD_CPUID_PARAM		"<leaf> [subleaf]"
#define SHELL_CMD_CPUID_HELP		"Display the CPUID leaf [subleaf], in hexadecimal"

#define SHELL_CMD_RDMSR			"rdmsr"
#define SHELL_CMD_RDMSR_PARAM		"[-p<pcpu_id>]	<msr_index>"
#define SHELL_CMD_RDMSR_HELP		"Read the MSR at msr_index (in hexadecimal) for CPU ID pcpu_id"

#define SHELL_CMD_WRMSR			"wrmsr"
#define SHELL_CMD_WRMSR_PARAM		"[-p<pcpu_id>]	<msr_index> <value>"
#define SHELL_CMD_WRMSR_HELP		"Write value (in hexadecimal) to the MSR at msr_index (in hexadecimal) for CPU"\
					" ID pcpu_id"
#endif /* SHELL_PRIV_H */
