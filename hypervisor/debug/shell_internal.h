/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SHELL_INTER_H
#define SHELL_INTER_H

#include <spinlock.h>

struct shell;


#define SHELL_CMD_MAX_LEN		100U
#define SHELL_NAME_MAX_LEN		50
#define SHELL_PARA_MAX_LEN		64
#define SHELL_HELP_MAX_LEN		256
#define SHELL_STRING_MAX_LEN		(CPU_PAGE_SIZE << 2)

/* Shell Control Block */
struct shell_cmd;
struct shell {
	char input_line[2][SHELL_CMD_MAX_LEN + 1U];	/* current & last */
	char name[SHELL_NAME_MAX_LEN];	/* Session name */
	uint32_t input_line_len;	/* Length of current input line */
	uint32_t input_line_active;	/* Active input line index */
	struct shell_cmd *shell_cmd;	/* cmds supported */
	uint32_t cmd_count;		/* Count of cmds supported */
};

/* Shell Command Function */
typedef int (*shell_cmd_fn_t)(int, char **);

/* Shell Command */
struct shell_cmd {
	char *str;		/* Command string */
	char *cmd_param;	/* Command parameter string */
	char *help_str;		/* Help text associated with the command */
	shell_cmd_fn_t fcn;	/* Command call-back function */

};

/* Shell Command list with parameters and help description */
#define SHELL_CMD_HELP			"help"
#define SHELL_CMD_HELP_PARAM		NULL
#define SHELL_CMD_HELP_HELP		"Display info about the supported shell commands."

#define SHELL_CMD_VM_LIST		"vm_list"
#define SHELL_CMD_VM_LIST_PARAM		NULL
#define SHELL_CMD_VM_LIST_HELP		"Lists all VMs (VM Name, VM ID, VM State)"

#define SHELL_CMD_VCPU_LIST		"vcpu_list"
#define SHELL_CMD_VCPU_LIST_PARAM	NULL
#define SHELL_CMD_VCPU_LIST_HELP	"Lists all VCPU in all VMs"

#define SHELL_CMD_VCPU_DUMPREG		"vcpu_dumpreg"
#define SHELL_CMD_VCPU_DUMPREG_PARAM	"<vm id, vcpu id>"
#define SHELL_CMD_VCPU_DUMPREG_HELP	"Dump registers for a specific vcpu"

#define SHELL_CMD_DUMPMEM		"dumpmem"
#define SHELL_CMD_DUMPMEM_PARAM		"<addr, length>"
#define SHELL_CMD_DUMPMEM_HELP		"Dump physical memory"

#define SHELL_CMD_SOS_CONSOLE		"sos_console"
#define SHELL_CMD_SOS_CONSOLE_PARAM	NULL
#define SHELL_CMD_SOS_CONSOLE_HELP	"Switch to SOS's console"

#define SHELL_CMD_INTERRUPT		"int"
#define SHELL_CMD_INTERRUPT_PARAM	NULL
#define SHELL_CMD_INTERRUPT_HELP	"show interrupt info per CPU"

#define SHELL_CMD_PTDEV			"pt"
#define SHELL_CMD_PTDEV_PARAM		NULL
#define SHELL_CMD_PTDEV_HELP		"show pass-through device info"

#define SHELL_CMD_REBOOT		"reboot"
#define SHELL_CMD_REBOOT_PARAM		NULL
#define SHELL_CMD_REBOOT_HELP		"trigger system reboot"

#define SHELL_CMD_IOAPIC		"dump_ioapic"
#define SHELL_CMD_IOAPIC_PARAM		NULL
#define SHELL_CMD_IOAPIC_HELP		"show native ioapic info"

#define SHELL_CMD_VIOAPIC		"vioapic"
#define SHELL_CMD_VIOAPIC_PARAM		"<vm id>"
#define SHELL_CMD_VIOAPIC_HELP		"show vioapic info"

#define SHELL_CMD_VMEXIT		"vmexit"
#define SHELL_CMD_VMEXIT_PARAM		NULL
#define SHELL_CMD_VMEXIT_HELP		"show vmexit profiling"

#define SHELL_CMD_LOGDUMP		"logdump"
#define SHELL_CMD_LOGDUMP_PARAM		"<pcpu id>"
#define SHELL_CMD_LOGDUMP_HELP		"log buffer dump"

#define SHELL_CMD_LOG_LVL		"loglevel"
#define SHELL_CMD_LOG_LVL_PARAM		"[console_loglevel] [mem_loglevel]"
#define SHELL_CMD_LOG_LVL_HELP		"get(para is NULL), or set loglevel [0-6]"

#define SHELL_CMD_CPUID			"cpuid"
#define SHELL_CMD_CPUID_PARAM		"<leaf> [subleaf]"
#define SHELL_CMD_CPUID_HELP		"cpuid leaf [subleaf], in hexadecimal"

/* Global function prototypes */
int shell_show_req_info(__unused int argc, __unused char **argv);
int shell_cmd_help(__unused int argc, __unused char **argv);
int shell_list_vm(__unused int argc, __unused char **argv);
int shell_list_vcpu(__unused int argc, __unused char **argv);
int shell_vcpu_dumpreg(int argc, char **argv);
int shell_dumpmem(int argc, char **argv);
int shell_to_sos_console(int argc, char **argv);
int shell_show_cpu_int(__unused int argc, __unused char **argv);
int shell_show_ptdev_info(__unused int argc, __unused char **argv);
int shell_reboot(__unused int argc, __unused char **argv);
int shell_show_vioapic_info(int argc, char **argv);
int shell_show_ioapic_info(__unused int argc, __unused char **argv);
int shell_show_vmexit_profile(__unused int argc, __unused char **argv);
int shell_dump_logbuf(int argc, char **argv);
int shell_loglevel(int argc, char **argv);
int shell_cpuid(int argc, char **argv);
struct shell_cmd *shell_find_cmd(const char *cmd);
int shell_process_cmd(char *p_input_line);
void shell_puts(const char *str_ptr);
int shell_trigger_crash(int argc, char **argv);

#endif /* SHELL_INTER_H */
