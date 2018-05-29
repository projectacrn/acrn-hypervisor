/*
 * Project Acrn
 * Acrn-dm-monitor
 *
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
 *
 *
 * Author: TaoYuhong <yuhong.tao@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>
#include "dm.h"
#include "monitor.h"
#include "acrn_mngr.h"

/* helpers */
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

struct vm_ops {
	char name[16];
	void *arg;
	struct monitor_vm_ops *ops;
	LIST_ENTRY(vm_ops) list;
};

static unsigned wakeup_reason = 0;

unsigned get_wakeup_reason(void)
{
	return wakeup_reason;
}

int set_wakeup_timer(time_t t)
{
	int acrnd_fd;
	struct req_acrnd_timer req;
	struct ack_acrnd_timer ack;
	int ret;

	acrnd_fd = mngr_open_un("acrnd", MNGR_CLIENT);
	if (acrnd_fd < 0) {
		return -1;
	}

	req.msg.magic = MNGR_MSG_MAGIC;
	req.msg.msgid = ACRND_TIMER;
	req.msg.timestamp = time(NULL);
	req.msg.len = sizeof(req);

	req.t = t;
	strncpy(req.name, vmname, sizeof(req.name));

	ret = mngr_send_msg(acrnd_fd, &req.msg, &ack.msg, sizeof(ack), 2);
	mngr_close(acrnd_fd);
	if (ret != sizeof(ack)) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	return ack.err;
}

static LIST_HEAD(vm_ops_list, vm_ops) vm_ops_head;
static pthread_mutex_t vm_ops_mtx = PTHREAD_MUTEX_INITIALIZER;

int monitor_register_vm_ops(struct monitor_vm_ops *mops, void *arg,
			    const char *name)
{
	struct vm_ops *ops;

	if (!mops) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	ops = calloc(1, sizeof(*ops));
	if (!ops) {
		perror("Alloc ops");
		return -1;
	}

	if (name)
		strncpy(ops->name, name, sizeof(ops->name) - 1);

	ops->ops = mops;
	ops->arg = arg;

	pthread_mutex_lock(&vm_ops_mtx);
	LIST_INSERT_HEAD(&vm_ops_head, ops, list);
	pthread_mutex_unlock(&vm_ops_mtx);

	return 0;
}

static int monitor_fd = -1;

/* handlers */
#define ACK_TIMEOUT	1

#define DEFINE_HANDLER(name, type, func)				\
static void name(struct mngr_msg *msg, int client_fd, void *param)	\
{									\
	struct req_##type *req = (void*)msg;			\
	struct ack_##type ack;					\
	struct vm_ops *ops;					\
								\
	int ret = 0;						\
	int count = 0;						\
								\
        memcpy(&ack.msg, &req->msg, sizeof(req->msg));		\
        ack.msg.len = sizeof(ack);				\
								\
	LIST_FOREACH(ops, &vm_ops_head, list) {			\
		if (ops->ops->func) {				\
			ret += ops->ops->func(ops->arg);	\
			count++;				\
		}						\
	}							\
								\
	if (!count) {						\
		ack.err = -1;					\
		fprintf(stderr, "No handler for id:%u\r\n", req->msg.msgid);	\
	} else									\
		ack.err = ret;							\
										\
	mngr_send_msg(client_fd, &ack.msg, NULL, 0, ACK_TIMEOUT);		\
}

DEFINE_HANDLER(handle_stop, dm_stop, stop);
DEFINE_HANDLER(handle_suspend, dm_suspend, suspend);
DEFINE_HANDLER(handle_pause, dm_pause, pause);
DEFINE_HANDLER(handle_continue, dm_continue, unpause);

static void handle_resume(struct mngr_msg *msg, int client_fd, void *param)
{
	struct req_dm_resume *req = (void *)msg;
	struct ack_dm_resume ack;
	struct vm_ops *ops;
	int ret = 0;
	int count = 0;

	memcpy(&ack.msg, &req->msg, sizeof(req->msg));
	ack.msg.len = sizeof(ack);

	LIST_FOREACH(ops, &vm_ops_head, list) {
		if (ops->ops->resume) {
			ret += ops->ops->resume(ops->arg);
			count++;
		}
	}

	if (!count) {
		ack.err = -1;
		fprintf(stderr, "No handler for id:%u\r\n", req->msg.msgid);
	} else
		ack.err = ret;

	wakeup_reason = req->reason;

	mngr_send_msg(client_fd, &ack.msg, NULL, 0, ACK_TIMEOUT);
}

static void handle_query(struct mngr_msg *msg, int client_fd, void *param)
{
	struct req_dm_query *req = (void *)msg;
	struct ack_dm_query ack;
	struct vm_ops *ops;

	memcpy(&ack.msg, &req->msg, sizeof(req->msg));
	ack.msg.len = sizeof(ack);

	ack.state = -1;

	LIST_FOREACH(ops, &vm_ops_head, list) {
		if (ops->ops->query) {
			ack.state = ops->ops->query(ops->arg);
			break;
		}
	}

	mngr_send_msg(client_fd, &ack.msg, NULL, 0, ACK_TIMEOUT);
}

int monitor_init(struct vmctx *ctx)
{
	int ret;
	char path[128] = {};

	ret = check_dir("/run/acrn/");
	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto dir_err;
	}

	ret = check_dir("/run/acrn/mngr");
	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto dir_err;
	}

	snprintf(path, sizeof(path) - 1, "%s.monitor", vmname);

	monitor_fd = mngr_open_un(path, MNGR_SERVER);
	if (monitor_fd < 0) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto server_err;
	}

	ret = 0;
	ret += mngr_add_handler(monitor_fd, DM_STOP, handle_stop, NULL);
	ret += mngr_add_handler(monitor_fd, DM_SUSPEND, handle_suspend, NULL);
	ret += mngr_add_handler(monitor_fd, DM_RESUME, handle_resume, NULL);
	ret += mngr_add_handler(monitor_fd, DM_PAUSE, handle_pause, NULL);
	ret += mngr_add_handler(monitor_fd, DM_CONTINUE, handle_continue, NULL);
	ret += mngr_add_handler(monitor_fd, DM_QUERY, handle_query, NULL);

	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto handlers_err;
	}

	return 0;

 handlers_err:
	mngr_close(monitor_fd);
	monitor_fd = -1;
 server_err:
 dir_err:
	return -1;
}

void monitor_close(void)
{
	if (monitor_fd >= 0)
		mngr_close(monitor_fd);
}
