/*
 * ProjectAcrn 
 * Acrnctl
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *
 * Author: Tao Yuhong <yuhong.tao@intel.com>
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "acrn_mngr.h"

#define ACRNCTL_OPT_ROOT	"/opt/acrn/conf"
#define MAX_NAME_LEN            (128)

/* helper functions */
static int shell_cmd(const char *cmd, char *outbuf, int len)
{
	FILE *ptr;
	char cmd_buf[256];
	int ret;

	if (!outbuf)
		return system(cmd);

	memset(cmd_buf, 0, sizeof(cmd_buf));
	memset(outbuf, 0, len);
	snprintf(cmd_buf, sizeof(cmd_buf), "%s 2>&1", cmd);
	ptr = popen(cmd_buf, "re");
	if (!ptr)
		return -1;

	ret = fread(outbuf, 1, len, ptr);
	pclose(ptr);

	return ret;
}

static void process_msg(struct mngr_msg *msg)
{
	if (msg->len < sizeof(*msg))
		return;

	switch(msg->msgid) {
	case MSG_STR:
		printf("%s\n", msg->payload);
		break;
	default:
		printf("Unknown msgid(%d) received\n", msg->msgid);
	}
}

/* vm states data and helper functions */

#define ACRN_DM_SOCK_ROOT	"/run/acrn/mngr"

struct vmm_struct {
	char name[MAX_NAME_LEN];
	unsigned long state;
	LIST_ENTRY(vmm_struct) list;
};

enum vm_state {
	VM_STATE_UNKNOWN = 0,
	VM_CREATED,		/* VM created / awaiting start (boot) */
	VM_STARTED,		/* VM started (booted) */
	VM_PAUSED,		/* VM paused */
	VM_UNTRACKED,		/* VM not created by acrnctl, or its launch script can change vm name */
};

static const char *state_str[] = {
	[VM_STATE_UNKNOWN] = "unknown",
	[VM_CREATED] = "stopped",
	[VM_STARTED] = "started",
	[VM_PAUSED] = "paused",
	[VM_UNTRACKED] = "untracked",
};

static LIST_HEAD(vmm_list_struct, vmm_struct) vmm_head;

static struct vmm_struct *vmm_list_add(char *name)
{
	struct vmm_struct *s;

	s = calloc(1, sizeof(struct vmm_struct));
	if (!s) {
		perror("alloc vmm_struct");
		return NULL;
	}

	strncpy(s->name, name, MAX_NAME_LEN - 1);
	LIST_INSERT_HEAD(&vmm_head, s, list);

	return s;
}

static struct vmm_struct *vmm_find(char *name)
{
	struct vmm_struct *s;

	LIST_FOREACH(s, &vmm_head, list)
	    if (!strcmp(name, s->name))
		return s;
	return NULL;
}

static void vmm_update(void)
{
	char cmd[128] = { };
	char cmd_out[256] = { };
	char *vmname;
	char *pvmname = NULL;
	struct vmm_struct *s;
	size_t len = sizeof(cmd_out);

	snprintf(cmd, sizeof(cmd),
		 "find %s/add/ -name \"*.sh\" | "
		 "sed \"s/\\/opt\\/acrn\\/conf\\/add\\///g\" | "
		 "sed \"s/.sh//g\"", ACRNCTL_OPT_ROOT);
	shell_cmd(cmd, cmd_out, sizeof(cmd_out));

	/* Properly null-terminate cmd_out */
	cmd_out[len - 1] = '\0';

	vmname = strtok_r(cmd_out, "\n", &pvmname);
	while (vmname) {
		s = vmm_list_add(vmname);
		if (!s)
			continue;
		s->state = VM_CREATED;
		vmname = strtok_r(NULL, "\n", &pvmname);
	}

	pvmname = NULL;

	snprintf(cmd, sizeof(cmd),
			"find %s/ -name \"*monitor.*.socket\" | "
			"sed \"s/\\/run\\/acrn\\/mngr\\///g\" | "
			"awk -F. \'{ print $1 }\'", ACRN_DM_SOCK_ROOT);
	shell_cmd(cmd, cmd_out, sizeof(cmd_out));

	/* Properly null-terminate cmd_out */
	cmd_out[len - 1] = '\0';

	vmname = strtok_r(cmd_out, "\n", &pvmname);
	while (vmname) {
		s = vmm_find(vmname);
		if (s)
			s->state = VM_STARTED;
		else {
			s = vmm_list_add(vmname);
			if (s)
				s->state = VM_UNTRACKED;
		}
		vmname = strtok_r(NULL, "\n", &pvmname);
	}
}

/* There are acrnctl cmds */
/* command: list */
static void acrnctl_list_help(void)
{
	printf("acrnctl list\n"
	       "\tlist all VMs, shown in %s or %s\n",
	       ACRNCTL_OPT_ROOT, ACRN_DM_SOCK_ROOT);
}

static int acrnctl_do_list(int argc, char *argv[])
{
	struct vmm_struct *s;
	int find = 0;

	if (argc == 2)
		if (!strcmp("help", argv[1])) {
			acrnctl_list_help();
			return 0;
		}

	vmm_update();
	LIST_FOREACH(s, &vmm_head, list) {
		printf("%s\t\t%s\n", s->name, state_str[s->state]);
		find++;
	}

	if (!find)
		printf("There are no VMs\n");

	return 0;
}

/* command: add */
static void acrnctl_add_help(void)
{
	printf("acrnctl add [uos_bash_script]\n"
	       "\trequires an uos script file\n");
}

static int check_name(const char *name)
{
	int i = 0, j = 0;
	char illegal[] = "!@#$%^&*, ";

	/* Name should start with a letter */
	if ((name[0] < 'a' || name[0] > 'z')
	    && (name[0] < 'A' || name[0] > 'Z')) {
		printf("name not started with latter!\n");
		return -1;
	}

	/* no illegal charactoer */
	while (name[i]) {
		j = 0;
		while (illegal[j]) {
			if (name[i] == illegal[j]) {
				printf("vmname[%d] is '%c'!\n", i, name[i]);
				return -1;
			}
			j++;
		}
		i++;
	}

	if (!strcmp(name, "help"))
		return -1;
	if (!strcmp(name, "nothing"))
		return -1;

	return 0;
}

static const char *acrnctl_bin_path;
static int find_acrn_dm;

static int write_tmp_file(int fd, int n, char *word[])
{
	int len, ret, i = 0;
	char buf[128];

	if (!n)
		return 0;

	len = strlen(word[0]);
	if (len >= strlen("acrn-dm")) {
		if (!strcmp(word[0] + len - strlen("acrn-dm"), "acrn-dm")) {
			find_acrn_dm++;
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%s gentmpfile",
				 acrnctl_bin_path);
			ret = write(fd, buf, strlen(buf));
			if (ret < 0)
				return -1;
			i++;
		}
	}

	while (i < n) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), " %s", word[i]);
		i++;
		ret = write(fd, buf, strlen(buf));
		if (ret < 0)
			return -1;
	}
	ret = write(fd, "\n", 1);
	if (ret < 0)
		return -1;
	return 0;
}

#define MAX_FILE_SIZE   (4096 * 4)
#define MAX_WORD	64
#define FILE_NAME_LENGTH	128

#define TMP_FILE_SUFFIX		".acrnctl"

static int acrnctl_do_add(int argc, char *argv[])
{
	struct vmm_struct *s;
	int fd, fd_tmp, ret = 0;
	char *buf;
	char *word[MAX_WORD], *line;
	char *word_p = NULL, *line_p = NULL;
	int n_word;
	char fname[FILE_NAME_LENGTH + sizeof(TMP_FILE_SUFFIX)];
	char cmd[128];
	char args[128];
	int p, i;
	char cmd_out[256];
	char vmname[128];
	size_t len = sizeof(cmd_out);

	if (argc < 2) {
		acrnctl_add_help();
		return -1;
	}

	if (!strcmp("help", argv[1])) {
		acrnctl_add_help();
		return 0;
	}

	if (strlen(argv[1]) >= FILE_NAME_LENGTH) {
		printf("file name too long: %s\n", argv[1]);
		return -1;
	}

	memset(args, 0, sizeof(args));
	p = 0;
	for (i = 2; i < argc; i++) {
		if (p >= sizeof(args) - 1) {
			args[sizeof(args) - 1] = 0;
			printf("Too many optional args: %s\n", args);
			return -1;
		}
		p += snprintf(&args[p], sizeof(args) - p, " %s", argv[i]);
	}
	args[p] = ' ';

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		ret = -1;
		goto open_read_file;
	}

	buf = calloc(1, MAX_FILE_SIZE);
	if (!buf) {
		perror("calloc for add vm");
		ret = -1;
		goto calloc_err;
	}

	ret = read(fd, buf, MAX_FILE_SIZE);
	if (ret >= MAX_FILE_SIZE) {
		printf("%s exceed MAX_FILE_SIZE:%d", argv[1], MAX_FILE_SIZE);
		ret = -1;
		goto file_exceed;
	}

	/* open tmp file for write */
	memset(fname, 0, sizeof(fname));
	snprintf(fname, sizeof(fname), "%s%s", argv[1], TMP_FILE_SUFFIX);
	fd_tmp = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fd_tmp < 0) {
		perror(fname);
		ret = -1;
		goto open_tmp_file;
	}

	find_acrn_dm = 0;

	/* Properly null-terminate buf */
	buf[MAX_FILE_SIZE - 1] = '\0';

	line = strtok_r(buf, "\n", &line_p);
	while (line) {
		word_p = NULL;
		n_word = 0;
		word[n_word] = strtok_r(line, " ", &word_p);
		while (word[n_word]) {
			n_word++;
			word[n_word] = strtok_r(NULL, " ", &word_p);
		}
		if (write_tmp_file(fd_tmp, n_word, word)) {
			ret = -1;
			perror(fname);
			goto write_tmpfile;
		}
		line = strtok_r(NULL, "\n", &line_p);
	}

	if (!find_acrn_dm) {
		printf
		    ("Don't see 'acrn-dm' in %s, maybe it is in another script, "
		     "this is no supported for now\n", argv[1]);
		ret = -1;
		goto no_acrn_dm;
	}

	snprintf(cmd, sizeof(cmd), "mv %s %s.back", argv[1], argv[1]);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "mv %s %s", fname, argv[1]);
	system(cmd);

	memset(vmname, 0, sizeof(vmname));
	snprintf(cmd, sizeof(cmd), "bash %s%s >./%s.result", argv[1],
		 args, argv[1]);
	ret = shell_cmd(cmd, cmd_out, sizeof(cmd_out));
	if (ret < 0)
		goto get_vmname;

	snprintf(cmd, sizeof(cmd), "grep -a \"acrnctl: \" ./%s.result", argv[1]);
	ret = shell_cmd(cmd, cmd_out, sizeof(cmd_out));
	if (ret < 0)
		goto get_vmname;

	ret = sscanf(cmd_out, "acrnctl: %s", vmname);
	if (ret != 1) {
		ret = -1;
		snprintf(cmd, sizeof(cmd), "cat ./%s.result", argv[1]);
		shell_cmd(cmd, cmd_out, sizeof(cmd_out));

		/* Properly null-terminate cmd_out */
		cmd_out[len - 1] = '\0';

		printf("%s can't reach acrn-dm, "
		       "please try again when you make sure it can launch an UOS\n"
		       "result:\n%s\n", argv[1], cmd_out);
		goto get_vmname;
	}

	ret = check_name(vmname);
	if (ret) {
		printf("\"%s\" is a bad name, please select another name\n",
		       vmname);
		goto get_vmname;
	}

	snprintf(cmd, sizeof(cmd), "mkdir -p %s/add", ACRNCTL_OPT_ROOT);
	system(cmd);

	vmm_update();
	s = vmm_find(vmname);
	if (s) {
		printf("%s(%s) already exist, can't add %s%s\n",
		       vmname, state_str[s->state], argv[1], args);
		ret = -1;
		goto vm_exist;
	}

	snprintf(cmd, sizeof(cmd), "cp %s.back %s/add/%s.sh", argv[1],
		 ACRNCTL_OPT_ROOT, vmname);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "echo %s >%s/add/%s.args", args,
		 ACRNCTL_OPT_ROOT, vmname);
	system(cmd);
	printf("%s added\n", vmname);

 vm_exist:
 get_vmname:
	snprintf(cmd, sizeof(cmd), "rm -f ./%s.result", argv[1]);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "mv %s %s", argv[1], fname);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "mv %s.back %s", argv[1], argv[1]);
	system(cmd);

 no_acrn_dm:
	snprintf(cmd, sizeof(cmd), "rm -f %s", fname);
	system(cmd);
 write_tmpfile:
	close(fd_tmp);
 open_tmp_file:
 file_exceed:
	free(buf);
 calloc_err:
	close(fd);
 open_read_file:
	return ret;
}

/* command: stop */
static void acrnctl_stop_help(void)
{
	printf("acrnctl stop [vmname0] [vmname1] ...\n"
	       "\t run \"acrnctl list\" to get running VMs\n");
}

static int send_stop_msg(char *vmname)
{
	int fd, ret;
	struct sockaddr_un addr;
	struct mngr_msg msg;
	struct timeval timeout;
	fd_set rfd, wfd;
	char buf[128];

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto sock_err;
	}

	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s-monitor.socket",
		 ACRN_DM_SOCK_ROOT, vmname);

	ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		goto connect_err;
	}

	msg.magic = MNGR_MSG_MAGIC;
	msg.msgid = DM_STOP;
	msg.len = sizeof(msg);

	timeout.tv_sec = 1;	/* wait 1 second for read/write socket */
	timeout.tv_usec = 0;
	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	FD_SET(fd, &rfd);
	FD_SET(fd, &wfd);

	select(fd + 1, NULL, &wfd, NULL, &timeout);

	if (!FD_ISSET(fd, &wfd)) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		goto cant_write;
	}

	ret = write(fd, &msg, sizeof(msg));

	/* wait response */
	select(fd + 1, &rfd, NULL, NULL, &timeout);

	if (FD_ISSET(fd, &rfd)) {
		memset(buf, 0, sizeof(buf));
		ret = read(fd, buf, sizeof(buf));
		if (ret <= sizeof(buf))
			process_msg((void*)&buf);
	}

 cant_write:
 connect_err:
	close(fd);
 sock_err:
	return ret;
}

static int acrnctl_do_stop(int argc, char *argv[])
{
	struct vmm_struct *s;
	int i;

	if (argc < 2) {
		acrnctl_stop_help();
		return -1;
	}

	if (!strcmp("help", argv[1])) {
		acrnctl_stop_help();
		return 0;
	}

	vmm_update();
	for (i = 1; i < argc; i++) {
		s = vmm_find(argv[i]);
		if (!s) {
			printf("can't find %s\n", argv[i]);
			continue;
		}
		if (s->state == VM_CREATED) {
			printf("%s is already (%s)\n", argv[i],
			       state_str[s->state]);
			continue;
		}
		send_stop_msg(argv[i]);
	}

	return 0;
}

/* command: delete */
static void acrnctl_del_help(void)
{
	printf("acrnctl del [vmname0] [vmname1] ...\n"
	       "\t run \"acrnctl list\" get VM names\n");
}

static int acrnctl_do_del(int argc, char *argv[])
{
	struct vmm_struct *s;
	int i;
	char cmd[128];

	if (argc < 2) {
		acrnctl_del_help();

		return -1;
	}

	if (!strcmp("help", argv[1])) {
		acrnctl_del_help();
		return 0;
	}

	vmm_update();
	for (i = 1; i < argc; i++) {
		s = vmm_find(argv[i]);
		if (!s) {
			printf("can't find %s\n", argv[i]);
			continue;
		}
		if (s->state != VM_CREATED) {
			printf("can't delete %s(%s)\n", argv[i],
			       state_str[s->state]);
			continue;
		}
		snprintf(cmd, sizeof(cmd), "rm -f %s/add/%s.sh",
			 ACRNCTL_OPT_ROOT, argv[i]);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "rm -f %s/add/%s.args",
			 ACRNCTL_OPT_ROOT, argv[i]);
		system(cmd);
	}

	return 0;
}

/* command: start */
static void acrnctl_start_help(void)
{
	printf("acrnctl start [vmname]\n"
	       "\t run \"acrnctl list\" get VM names\n"
	       "\t each time user can only start one VM\n");
}

static int acrnctl_do_start(int argc, char *argv[])
{
	struct vmm_struct *s;
	char cmd[128];

	if (argc != 2) {
		acrnctl_start_help();
		return -1;
	}

	if (!strcmp("help", argv[1])) {
		acrnctl_start_help();
		return 0;
	}

	vmm_update();
	s = vmm_find(argv[1]);
	if (!s) {
		printf("can't find %s\n", argv[1]);
		return -1;
	}

	if (s->state != VM_CREATED) {
		printf("can't start %s(%s)\n", argv[1], state_str[s->state]);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "bash %s/add/%s.sh $(cat %s/add/%s.args)",
		 ACRNCTL_OPT_ROOT, argv[1], ACRNCTL_OPT_ROOT, argv[1]);

	system(cmd);

	return 0;
}

#define ACMD(CMD,FUNC)	\
{.cmd = CMD, .func = FUNC,}

struct acrnctl_cmd {
	const char *cmd;
	int (*func) (int argc, char *argv[]);
} acmds[] = {
	ACMD("list", acrnctl_do_list),
	ACMD("start", acrnctl_do_start),
	ACMD("stop", acrnctl_do_stop),
	ACMD("del", acrnctl_do_del),
	ACMD("add", acrnctl_do_add),
};

#define NCMD	(sizeof(acmds)/sizeof(struct acrnctl_cmd))

static void help_info(void)
{
	int i;

	printf("support:\n");
	for (i = 0; i < NCMD; i++)
		printf("\t%s\n", acmds[i].cmd);
	printf("Use acrnctl [cmd] help for details\n");
}

int main(int argc, char *argv[])
{
	int i;

	if (argc == 1) {
		help_info();
		return 0;
	}

	acrnctl_bin_path = argv[0];

	/* first check acrnctl reserved operations */
	if (!strcmp(argv[1], "gentmpfile")) {
		printf("\nacrnctl: %s\n", argv[argc - 1]);
		return 0;
	}

	for (i = 0; i < NCMD; i++)
		if (!strcmp(argv[1], acmds[i].cmd))
			return acmds[i].func(argc - 1, &argv[1]);

	/* Reach here means unsupported command */
	printf("Unknown command: %s\n", argv[1]);
	help_info();

	return -1;
}
