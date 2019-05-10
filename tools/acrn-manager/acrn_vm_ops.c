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
#include <limits.h>
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
	[VM_SUSPENDED] = "suspended",
	[VM_UNTRACKED] = "untracked",
};

/* List head of all vm */
static pthread_mutex_t vmmngr_mutex = PTHREAD_MUTEX_INITIALIZER;
struct vmmngr_list_struct vmmngr_head = { NULL };
static unsigned long update_count = 0;

struct vmmngr_struct *vmmngr_find(const char *name)
{
	struct vmmngr_struct *s;

	LIST_FOREACH(s, &vmmngr_head, list)
	    if (!strcmp(name, s->name))
		return s;
	return NULL;
}

static int send_msg(const char *vmname, struct mngr_msg *req, struct mngr_msg *ack);

static int query_state(const char *name)
{
	struct mngr_msg req;
	struct mngr_msg ack;
	int ret;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_QUERY;
	req.timestamp = time(NULL);

	ret = send_msg(name, &req, &ack);
	if (ret) {
		printf("%s: Error to quary %s state, err: %d\n", __func__, name, ret);
		return ret;
	}

	if (ack.data.state < 0) {
		fprintf(stderr, "%s ack.data.state:%d\n", __FUNCTION__, ack.data.state);
	}

	return ack.data.state;
}

/*
 * get vmname and pid from /run/acrn/mngr/[vmname].monitor.[pid].socket
 */
static inline int _get_vmname_pid(const char *src, char *p_vmname,
		int max_len_vmname, int *pid)
{
	char *p = NULL;

	p = strchr(src, '.');
	/* p - src: length of the substring "vmname" in the sting "src" */
	if (!p || p - src == 0 || p - src >= max_len_vmname)
		return -1;
	else
		strncpy(p_vmname, src, p - src);

	/* move the pointer to the "pid" in the string "src" */
	if (strncmp(".monitor.", p, strlen(".monitor.")))
		return -1;
	else
		p = p + strlen(".monitor.");

	*pid = strtol(p, NULL, 10);
	if ((errno == ERANGE && (*pid == LONG_MAX || *pid == LONG_MIN))
			|| (errno != 0 && *pid == 0))
		return -1;

	p = strchr(p, '.');
	if (!p || strncmp(".socket", p, strlen(".socket")))
		return -1;

	return 0;
}

/* find all the running DM process, which has */
/* /run/acrn/mngr/[vmname].monitor.[pid].socket */
static void _scan_alive_vm(void)
{
	DIR *dir;
	struct dirent *entry;
	struct vmmngr_struct *vm;
	char name[PATH_LEN];
	int pid;
	int ret;

	ret = check_dir(ACRN_DM_SOCK_PATH, CHK_ONLY);
	if (ret) {
		printf("%s: Failed to check directory %s, err: %d\n", __func__, ACRN_DM_SOCK_PATH, ret);
		return;
	}

	dir = opendir(ACRN_DM_SOCK_PATH);
	if (!dir) {
		printf("%s: Failed to open directory %s\n", __func__, ACRN_DM_SOCK_PATH);
		return;
	}

	while ((entry = readdir(dir))) {
		memset(name, 0, sizeof(name));
		ret = _get_vmname_pid(entry->d_name, name, sizeof(name), &pid);
		if (ret < 0)
			continue;

		if (name[sizeof(name) - 1]) {
			/* truncate name and go a head */
			name[sizeof(name) - 1] = 0;
			printf("%s: Truncate name as %s\n", __func__, name);
		}

		vm = vmmngr_find(name);

		if (!vm) {
			vm = calloc(1, sizeof(*vm));
			if (!vm) {
				printf("%s: Failed to alloc mem for %s\n", __func__, name);
				continue;
			}
			memcpy(vm->name, name, sizeof(vm->name) - 1);
			LIST_INSERT_HEAD(&vmmngr_head, vm, list);
		}

		ret = query_state(name);

		if (ret < 0)
			/* unsupport query */
			vm->state_tmp = VM_STARTED;
		else
			switch (ret) {
			case VM_SUSPEND_NONE:
				vm->state_tmp = VM_STARTED;
				break;
			case VM_SUSPEND_SUSPEND:
				vm->state_tmp = VM_SUSPENDED;
				break;
			default:
				fprintf(stderr, "Warnning: unknow vm state:0x%lx\n",
										vm->state);
				vm->state_tmp = VM_STATE_UNKNOWN;
			}
		vm->update = update_count;
	}

	closedir(dir);
}

/*
 * get vmname and suffix from src,
 * which has [vmname].[suffix]
 */
static inline int _get_vmname_suffix(const char *src,
		char *name, int max_len_name, char *suffix, int max_len_suffix)
{
	char *p = NULL;

	p = strchr(src, '.');
	/* p - src: length of the substring vmname in the string src*/
	if (!p || p - src == 0)
		return -1;

	strncpy(name, src, p - src);
	if (p - src >= max_len_name) {
		/* truncate name and go a head */
		name[max_len_name - 1] = '\0';
		printf("%s: Truncate name as %s\n", __func__, name);
	}

	strncpy(suffix, p + 1, max_len_suffix);
	if (strncmp(suffix, "sh", strlen("sh")))
		return -1;

	return 0;
}

static void _scan_added_vm(void)
{
	DIR *dir;
	struct dirent *entry;
	struct vmmngr_struct *vm;
	char name[PATH_LEN];
	char suffix[PATH_LEN];
	int ret;

	ret = check_dir(ACRN_CONF_PATH, CHK_ONLY);
	if (ret) {
		printf("%s: Failed to check directory %s, err: %d\n", __func__, ACRN_CONF_PATH, ret);
		return;
	}

	ret = check_dir(ACRN_CONF_PATH_ADD, CHK_ONLY);
	if (ret) {
		printf("%s: Failed to check directory %s, err: %d\n", __func__, ACRN_CONF_PATH_ADD, ret);
		return;
	}

	dir = opendir(ACRN_CONF_PATH_ADD);
	if (!dir) {
		printf("%s: Failed to open directory %s\n", __func__, ACRN_CONF_PATH_ADD);
		return;
	}

	while ((entry = readdir(dir))) {
		ret = strnlen(entry->d_name, sizeof(entry->d_name));
		if (ret >= sizeof(name)) {
			continue;
		}

		memset(name, 0, sizeof(name));
		memset(suffix, 0, sizeof(suffix));
		ret = _get_vmname_suffix(entry->d_name,
				name, sizeof(name), suffix, sizeof(suffix));
		if (ret < 0)
			continue;

		vm = vmmngr_find(name);

		if (!vm) {
			vm = calloc(1, sizeof(*vm));
			if (!vm) {
				printf("%s: Failed to alloc mem for %s\n", __func__, name);
				continue;
			}
			memcpy(vm->name, name, sizeof(vm->name) - 1);
			LIST_INSERT_HEAD(&vmmngr_head, vm, list);
		}

		vm->state_tmp = VM_CREATED;
		vm->update = update_count;
	}

	closedir(dir);
}

static void _remove_dead_vm(void)
{
	struct vmmngr_struct *vm, *tvm;

	list_foreach_safe(vm, &vmmngr_head, list, tvm) {
		if (vm->update == update_count) {
			vm->state = vm->state_tmp;
			continue;
		}
		LIST_REMOVE(vm, list);
		printf("%s: Removed dead %s\n", __func__, vm->name);
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
	if (snprintf(cmd_buf, sizeof(cmd_buf), "%s 2>&1", cmd) >= sizeof(cmd_buf)) {
		printf("ERROR: shell command is truncated\n");
		return -1;
	}
	ptr = popen(cmd_buf, "re");
	if (!ptr)
		return -1;

	ret = fread(outbuf, 1, len, ptr);
	pclose(ptr);

	return ret;
}

static int send_msg(const char *vmname, struct mngr_msg *req,
		    struct mngr_msg *ack)
{
	int fd, ret;

	if (!vmname) {
		printf("No vmname provided\n");
		return -EINVAL;
	}

	fd = mngr_open_un(vmname, MNGR_CLIENT);
	if (fd < 0) {
		printf("Unable to open vm %s socket. It may have been shutdown\n", vmname);
		return -1;
	}

	ret = mngr_send_msg(fd, req, ack, 1);
	if (ret < 0) {
		printf("Unable to send msg to vm %s socket. It may have been shutdown\n", vmname);
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

int start_vm(const char *vmname)
{
	char cmd[PATH_LEN + sizeof(ACRN_CONF_PATH_ADD) * 2 + MAX_VM_OS_NAME_LEN * 2];

	if (snprintf(cmd, sizeof(cmd), "bash %s/%s.sh $(cat %s/%s.args)",
			ACRN_CONF_PATH_ADD, vmname, ACRN_CONF_PATH_ADD, vmname) >= sizeof(cmd)) {
		printf("ERROR: command is truncated\n");
		return -1;
	}

	return system(cmd);
}

int stop_vm(const char *vmname)
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

int pause_vm(const char *vmname)
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

int continue_vm(const char *vmname)
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

int suspend_vm(const char *vmname)
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

int resume_vm(const char *vmname, unsigned reason)
{
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_RESUME;
	req.timestamp = time(NULL);

	req.data.reason = reason;

	send_msg(vmname, &req, &ack);

	if (ack.data.err) {
		printf("Unable to resume vm. errno(%d)\n", ack.data.err);
	}

	return ack.data.err;
}

int blkrescan_vm(const char *vmname, char *devargs)
{
	struct mngr_msg req;
	struct mngr_msg ack;

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = DM_BLKRESCAN;
	req.timestamp = time(NULL);
	strncpy(req.data.devargs, devargs, PARAM_LEN - 1);
	req.data.devargs[PARAM_LEN - 1] = '\0';

	send_msg(vmname, &req, &ack);

	if (ack.data.err) {
		printf("Unable to rescan virtio-blk device in vm. errno(%d)\n", ack.data.err);
	}

	return ack.data.err;
}
