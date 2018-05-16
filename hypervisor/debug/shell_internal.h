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

#ifndef SHELL_INTER_H
#define SHELL_INTER_H

#include <spinlock.h>

struct shell;

/* Structure to hold the details about shell input and output */
struct shell_io {
	void *io_session_info;
	int (*io_init)(struct shell *);
	int (*io_deinit)(struct shell *);
	void (*io_puts)(struct shell *, char *);
	uint8_t (*io_getc)(struct shell *);
	void (*io_special)(struct shell *, uint8_t);
	bool io_echo_on;
};

#define SHELL_CMD_MAX_LEN		100
#define SHELL_NAME_MAX_LEN		50
#define SHELL_PARA_MAX_LEN		64
#define SHELL_HELP_MAX_LEN		256
#define SHELL_STRING_MAX_LEN		(CPU_PAGE_SIZE << 2)

/* Shell Control Block */
struct shell_cmd;
struct shell {
	struct shell_io session_io;	/* Session I/O information */
	char input_line[2][SHELL_CMD_MAX_LEN + 1];	/* current & last */
	char name[SHELL_NAME_MAX_LEN];	/* Session name */
	uint32_t input_line_len;	/* Length of current input line */
	uint32_t input_line_active;	/* Active input line index */
	struct shell_cmd *shell_cmd;	/* cmds supported */
	uint32_t cmd_count;		/* Count of cmds supported */
};

/* Shell Command Function */
typedef int (*shell_cmd_fn_t)(struct shell *, int, char **);

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

#define SHELL_CMD_VCPU_PAUSE		"vcpu_pause"
#define SHELL_CMD_VCPU_PAUSE_PARAM	"<vm id, vcpu id>"
#define SHELL_CMD_VCPU_PAUSE_HELP	"Pause a specific vcpu"

#define SHELL_CMD_VCPU_RESUME		"vcpu_resume"
#define SHELL_CMD_VCPU_RESUME_PARAM	"<vm id, vcpu id>"
#define SHELL_CMD_VCPU_RESUME_HELP	"Resume a specific vcpu"

#define SHELL_CMD_VCPU_DUMPREG		"vcpu_dumpreg"
#define SHELL_CMD_VCPU_DUMPREG_PARAM	"<vm id, vcpu id>"
#define SHELL_CMD_VCPU_DUMPREG_HELP	"Dump registers for a specific vcpu"

#define SHELL_CMD_VCPU_DUMPMEM		"vcpu_dumpmem"
#define SHELL_CMD_VCPU_DUMPMEM_PARAM	"<vcpu id, gva, length>"
#define SHELL_CMD_VCPU_DUMPMEM_HELP	"Dump memory for a specific vcpu"

#define SHELL_CMD_VM_CONSOLE		"vm_console"
#define SHELL_CMD_VM_CONSOLE_PARAM	NULL
#define SHELL_CMD_VM_CONSOLE_HELP	"Switch to SOS's console"

#define SHELL_CMD_INTERRUPT		"int"
#define SHELL_CMD_INTERRUPT_PARAM	NULL
#define SHELL_CMD_INTERRUPT_HELP	"show interrupt info per CPU"

#define SHELL_CMD_PTDEV			"pt"
#define SHELL_CMD_PTDEV_PARAM		NULL
#define SHELL_CMD_PTDEV_HELP		"show pass-through device info"

#define SHELL_CMD_REQ			"lsreq"
#define SHELL_CMD_REQ_PARAM		NULL
#define SHELL_CMD_REQ_HELP		"show ioreq info"

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

#define SHELL_CMD_trace			"trace"
#define SHELL_CMD_trace_PARAM		"<cpumask> <ms>"
#define SHELL_CMD_trace_HELP		"Dump cpus recent events within <ms> millisecond"

#define SHELL_CMD_GET_LOG_LVL		"get_loglevel"
#define SHELL_CMD_GET_LOG_LVL_PARAM	NULL
#define SHELL_CMD_GET_LOG_LVL_HELP	"Get the loglevel"

#define SHELL_CMD_SET_LOG_LVL		"set_loglevel"
#define SHELL_CMD_SET_LOG_LVL_PARAM	"<console_loglevel> [mem_loglevel]"
#define SHELL_CMD_SET_LOG_LVL_HELP	"Set loglevel [0-6]"

#define SHELL_CMD_CPUID			"cpuid"
#define SHELL_CMD_CPUID_PARAM		"<leaf> [subleaf]"
#define SHELL_CMD_CPUID_HELP		"cpuid leaf [subleaf], in hexadecimal"


/* Global function prototypes */
int shell_show_req_info(struct shell *p_shell, int argc, char **argv);
int shell_construct(struct shell **p_shell);
int shell_cmd_help(struct shell *p_shell, int argc, char **argv);
int shell_reset_cmd(struct shell *p_shell, int argc, char **argv);
int shell_list_vm(struct shell *p_shell, int argc, char **argv);
int shell_list_vcpu(struct shell *p_shell, int argc, char **argv);
int shell_pause_vcpu(struct shell *p_shell, int argc, char **argv);
int shell_resume_vcpu(struct shell *p_shell, int argc, char **argv);
int shell_vcpu_dumpreg(struct shell *p_shell, int argc, char **argv);
int shell_vcpu_dumpmem(struct shell *p_shell, int argc, char **argv);
int shell_boot_vm(struct shell *p_shell, int argc, char **argv);
int shell_trace_cmd(struct shell *p_shell, int argc, char **argv);
int shell_to_sos_console(struct shell *p_shell, int argc, char **argv);
int shell_show_cpu_int(struct shell *p_shell, int argc, char **argv);
int shell_show_ptdev_info(struct shell *p_shell, int argc, char **argv);
int shell_show_vioapic_info(struct shell *p_shell, int argc, char **argv);
int shell_show_ioapic_info(struct shell *p_shell, int argc, char **argv);
int shell_show_vmexit_profile(struct shell *p_shell, int argc, char **argv);
int shell_dump_logbuf(struct shell *p_shell, int argc, char **argv);
int shell_get_loglevel(struct shell *p_shell, int argc, char **argv);
int shell_set_loglevel(struct shell *p_shell, int argc, char **argv);
int shell_cpuid(struct shell *p_shell, int argc, char **argv);
struct shell_cmd *shell_find_cmd(struct shell *p_shell, const char *cmd);
int shell_process_cmd(struct shell *p_shell, char *p_input_line);
int shell_terminate_serial(struct shell *p_shell);
int shell_init_serial(struct shell *p_shell);
void shell_puts_serial(struct shell *p_shell, char *string_ptr);
uint8_t shell_getc_serial(struct shell *p_shell);
void shell_special_serial(struct shell *p_shell, uint8_t ch);
void kick_shell(struct shell *p_shell);

int shell_puts(struct shell *p_shell, char *str_ptr);
int shell_set_name(struct shell *p_shell, char *name);

#endif /* SHELL_INTER_H */
