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
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include "acrnctl.h"
#include "acrn_mngr.h"
#include "mevent.h"
#include "vmm.h"

const char *state_str[] = {
	[VM_STATE_UNKNOWN] = "unknown",
	[VM_CREATED] = "stopped",
	[VM_STARTED] = "started",
	[VM_PAUSED] = "paused",
	[VM_UNTRACKED] = "untracked",
};

/* Check if @path is a directory, and create if not exist */
static int check_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st)) {
		if (mkdir(path, 0666)) {
			perror(path);
			return -1;
		}
		return 0;
	}

	if (S_ISDIR(st.st_mode))
		return 0;

	fprintf(stderr, "%s exist, and not a directory!\n", path);
	return -1;
}

/* List head of all vm */
static pthread_mutex_t vmmngr_mutex = PTHREAD_MUTEX_INITIALIZER;
struct vmmngr_list_struct vmmngr_head;
static unsigned long update_count = 0;

struct vmmngr_struct *vmmngr_find(char *name)
{
	struct vmmngr_struct *s;

	LIST_FOREACH(s, &vmmngr_head, list)
	    if (!strcmp(name, s->name))
		return s;
	return NULL;
}

static int send_msg(char *vmname, struct mngr_msg *req, struct mngr_msg *ack);

static int query_state(const char *name)
{
	struct mngr_msg req;
	struct mngr_msg ack;
	int ret;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_QUERY;
	req.timestamp = time(NULL);

	ret = send_msg(vmname, &req, &ack);
	if (ret)
		return ret;

	if (ack.data.state < 0)
		pdebug();

	return ack.data.state;
}

/* find all the running DM process, which has */
/* /run/acrn/mngr/[vmname].monitor.[pid].socket */
static void _scan_alive_vm(void)
{
	DIR *dir;
	struct dirent *entry;
	struct vmmngr_struct *vm;
	char name[128];
	int pid;
	int ret;

	ret = check_dir(ACRN_DM_SOCK_ROOT);
	if (ret) {
		pdebug();
		return;
	}

	dir = opendir(ACRN_DM_SOCK_ROOT);
	if (!dir) {
		pdebug();
		return;
	}

	while ((entry = readdir(dir))) {
		memset(name, 0, sizeof(name));
		ret =
		    sscanf(entry->d_name, "%[^.].monitor.%d.socket", name,
			   &pid);
		if (ret != 2)
			continue;

		if (name[sizeof(name) - 1]) {
			pdebug();
			/* truncate name and go a head */
			name[sizeof(name) - 1] = 0;
		}

		vm = vmmngr_find(name);

		if (!vm) {
			vm = calloc(1, sizeof(*vm));
			if (!vm) {
				pdebug();
				continue;
			}
			memcpy(vm->name, name, sizeof(vm->name) - 1);
			LIST_INSERT_HEAD(&vmmngr_head, vm, list);
		}

		ret = query_state(name);

		if (ret < 0)
			/* unsupport query */
			vm->state = VM_STARTED;
		else
			switch (ret) {
			case VM_SUSPEND_NONE:
				vm->state = VM_STARTED;
				break;
			case VM_SUSPEND_HALT:
				vm->state = VM_PAUSED;
				break;
			default:
				vm->state = VM_STATE_UNKNOWN;
			}
		vm->update = update_count;
	}

}

static void _scan_added_vm(void)
{
	DIR *dir;
	struct dirent *entry;
	struct vmmngr_struct *vm;
	char name[128];
	char suffix[128];
	int ret;

	ret = check_dir(ACRNCTL_OPT_ROOT);
	if (ret) {
		pdebug();
		return;
	}

	ret = check_dir("/opt/acrn/conf/add");
	if (ret) {
		pdebug();
		return;
	}

	dir = opendir("/opt/acrn/conf/add");
	if (!dir) {
		pdebug();
		return;
	}

	while ((entry = readdir(dir))) {
		memset(name, 0, sizeof(name));
		memset(suffix, 0, sizeof(suffix));

		ret = strnlen(entry->d_name, sizeof(entry->d_name));
		if (ret >= sizeof(name)) {
			pdebug();
			continue;
		}

		ret = sscanf(entry->d_name, "%[^.].%s", name, suffix);

		if (ret != 2)
			continue;

		if (name[sizeof(name) - 1]) {
			pdebug();
			/* truncate name and go a head */
			name[sizeof(name) - 1] = 0;
		}

		if (strncmp(suffix, "sh", sizeof("sh")))
			continue;

		vm = vmmngr_find(name);

		if (!vm) {
			vm = calloc(1, sizeof(*vm));
			if (!vm) {
				pdebug();
				continue;
			}
			memcpy(vm->name, name, sizeof(vm->name) - 1);
			LIST_INSERT_HEAD(&vmmngr_head, vm, list);
		}

		vm->state = VM_CREATED;
		vm->update = update_count;
	}
}

static void _remove_dead_vm(void)
{
	struct vmmngr_struct *vm, *tvm;

	list_foreach_safe(vm, &vmmngr_head, list, tvm) {
		if (vm->update == update_count)
			continue;
		LIST_REMOVE(vm, list);
		pdebug();
		free(vm);
	}
};

void vmmngr_update(void)
{
	pthread_mutex_lock(&vmmngr_mutex);
	update_count++;
	_scan_added_vm();
	_scan_alive_vm();
	_remove_dead_vm();
	pthread_mutex_unlock(&vmmngr_mutex);
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
		    struct mngr_msg *ack)
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

	ret = mngr_send_msg(fd, req, ack, 1);
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
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_STOP;
	req.timestamp = time(NULL);

	send_msg(vmname, &req, &ack);
	if (ack.data.err) {
		printf("Error happens when try to stop vm. errno(%d)\n",
			ack.data.err);
	}

	return ack.data.err;
}

int pause_vm(char *vmname)
{
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_PAUSE;
	req.timestamp = time(NULL);

	send_msg(vmname, &req, &ack);
	if (ack.data.err) {
		printf("Unable to pause vm. errno(%d)\n", ack.data.err);
	}

	return ack.data.err;
}

int continue_vm(char *vmname)
{
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_CONTINUE;
	req.timestamp = time(NULL);

	send_msg(vmname, &req, &ack);

	if (ack.data.err) {
		printf("Unable to continue vm. errno(%d)\n", ack.data.err);
	}

	return ack.data.err;
}

int suspend_vm(char *vmname)
{
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_SUSPEND;
	req.timestamp = time(NULL);

	send_msg(vmname, &req, &ack);

	if (ack.data.err) {
		printf("Unable to suspend vm. errno(%d)\n", ack.data.err);
	}

	return ack.data.err;
}

int resume_vm(char *vmname)
{
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_RESUME;
	req.timestamp = time(NULL);

	send_msg(vmname, &req, &ack);

	if (ack.data.err) {
		printf("Unable to resume vm. errno(%d)\n", ack.data.err);
	}

	return ack.data.err;
}
