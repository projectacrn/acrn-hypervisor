/**
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "acrnctl.h"
#include "acrn_mngr.h"

/* List head of all vm */
static LIST_HEAD(vmmngr_list_struct, vmmngr_struct) vmmngr_head;

static struct vmmngr_struct *vmmngr_list_add(char *name)
{
	struct vmmngr_struct *s;

	s = calloc(1, sizeof(struct vmmngr_struct));
	if (!s) {
		perror("alloc vmmngr_struct");
		return NULL;
	}

	strncpy(s->name, name, MAX_NAME_LEN - 1);
	LIST_INSERT_HEAD(&vmmngr_head, s, list);

	return s;
}

struct vmmngr_struct *vmmngr_find(char *name)
{
	struct vmmngr_struct *s;

	LIST_FOREACH(s, &vmmngr_head, list)
	    if (!strcmp(name, s->name))
		return s;
	return NULL;
}

void get_vm_list(void)
{
	char cmd[128] = { };
	char cmd_out[256] = { };
	char *vmname;
	char *pvmname = NULL;
	struct vmmngr_struct *s;
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
		s = vmmngr_list_add(vmname);
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
		s = vmmngr_find(vmname);
		if (s)
			s->state = VM_STARTED;
		else {
			s = vmmngr_list_add(vmname);
			if (s)
				s->state = VM_UNTRACKED;
		}
		vmname = strtok_r(NULL, "\n", &pvmname);
	}
}

void put_vm_list(void)
{
	struct vmmngr_struct *s;

	while (!LIST_EMPTY(&vmmngr_head)) {
		s = LIST_FIRST(&vmmngr_head);
		LIST_REMOVE(s, list);
		free(s);
	}
}

/* helper functions */
int shell_cmd(const char *cmd, char *outbuf, int len)
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

static int send_msg(char *vmname, struct mngr_msg *req,
		    struct mngr_msg *ack, size_t ack_len)
{
	int fd, ret;

	if (!vmname) {
		printf("No vmname provided\n");
		return -EINVAL;
	}

	fd = mngr_open_un(vmname, MNGR_CLIENT);
	if (fd < 0) {
		printf("%s: Unable to open %s. line %d\n", __FUNCTION__,
		       vmname, __LINE__);
		return -1;
	}

	ret = mngr_send_msg(fd, req, ack, ack_len, 1);
	if (ret < 0) {
		printf("%s: Unable to send msg\n", __FUNCTION__);
		mngr_close(fd);
		return ret;
	}

	mngr_close(fd);

	return 0;
}

int list_vm()
{
	struct vmmngr_struct *s;
	int find = 0;

	LIST_FOREACH(s, &vmmngr_head, list) {
		printf("%s\t\t%s\n", s->name, state_str[s->state]);
		find++;
	}

	if (!find)
		printf("There are no VMs\n");

	return 0;
}

int start_vm(char *vmname)
{
	char cmd[128];

	snprintf(cmd, sizeof(cmd), "bash %s/add/%s.sh $(cat %s/add/%s.args)",
		 ACRNCTL_OPT_ROOT, vmname, ACRNCTL_OPT_ROOT, vmname);

	return system(cmd);
}

int stop_vm(char *vmname)
{
	struct req_dm_stop req;
	struct ack_dm_stop ack;

	req.msg.magic = MNGR_MSG_MAGIC;
	req.msg.msgid = DM_STOP;
	req.msg.timestamp = time(NULL);
	req.msg.len = sizeof(req);

	send_msg(vmname, (struct mngr_msg *)&req,
		       (struct mngr_msg *)&ack, sizeof(ack));
	if (ack.err) {
		printf("Error happens when try to stop vm. errno(%d)\n",
			ack.err);
	}

	return ack.err;
}
