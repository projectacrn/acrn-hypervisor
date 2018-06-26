/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <time.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <errno.h>
#include "mevent.h"
#include "acrnctl.h"
#include "acrn_mngr.h"
#include "ioc.h"

/* acrnd worker timer */

struct work_arg {
	char name[VMNAME_LEN];
};

struct acrnd_work {
	void (*func) (struct work_arg *arg);
	time_t expire;		/* when execute this work */

	struct work_arg arg;

	LIST_ENTRY(acrnd_work) list;
};

static LIST_HEAD(acrnd_work_list, acrnd_work) work_head;
static pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;

/* acrnd_add_work(), add a worker function.
 * @func, the worker function.
 * @sec, when add a @func(), after @sec seconds @func() will be called.
 * @arg, a cpoy of @arg will be pass to @func.
 */
int acrnd_add_work(void (*func) (struct work_arg *arg),
			struct work_arg *arg, unsigned sec)
{
	struct acrnd_work *work;
	time_t current;

	if (!func) {
		pdebug();
		return -1;
	}

	work = calloc(1, sizeof(*work));
	if (!work) {
		perror("Alloc work struct fail:");
		return -1;
	}

	current = time(NULL);
	if (current == (time_t) - 1) {
		perror("Get current time by timer() fail:");
		free(work);
		return -1;
	}

	work->func = func;
	work->expire = sec + current;
	if (arg)
		memcpy(&work->arg, arg, sizeof(*arg));

	pthread_mutex_lock(&work_mutex);
	LIST_INSERT_HEAD(&work_head, work, list);
	pthread_mutex_unlock(&work_mutex);
	return 0;
}

/* check if the works in work_head is expired, if expired then run
 * work func() once and delete it
 */
static void try_do_works(void)
{
	time_t current;
	struct acrnd_work *work, *twork;

	current = time(NULL);
	if (current == (time_t) - 1) {
		perror("Get current time by timer() fail:");
		return;
	}

	list_foreach_safe(work, &work_head, list, twork) {
		if (current > work->expire) {
			pthread_mutex_lock(&work_mutex);
			work->func(&work->arg);
			LIST_REMOVE(work, list);
			pthread_mutex_unlock(&work_mutex);
			free(work);
		}
	}
}

static void acrnd_run_vm(char *name);

/* Time to run/resume VM */
void acrnd_vm_timer_func(struct work_arg *arg)
{
	struct vmmngr_struct *vm;

	if (!arg) {
		pdebug();
		return;
	}

	vmmngr_update();
	vm = vmmngr_find(arg->name);
	if (!vm) {
		pdebug();
		return;
	}

	switch (vm->state) {
	case VM_CREATED:
		acrnd_run_vm(arg->name);
		break;
	case VM_PAUSED:
		resume_vm(arg->name);
		break;
	default:
		pdebug();
	}
}

#define TIMER_LIST_FILE "/opt/acrn/conf/timer_list"
static pthread_mutex_t timer_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* load/store_timer_list to file to keep timers if SOS poweroff */
static int load_timer_list(void)
{
	FILE *fp;
	struct work_arg arg = {};
	time_t expire;
	time_t record;
	time_t current;
	char l[256];
	char s1[16], s2[64], s3[64];	/* vmname & expire */
	int ret = 0;

	pthread_mutex_lock(&timer_file_mutex);

	fp = fopen(TIMER_LIST_FILE, "r");
	if (!fp) {
		perror("Open timer list file");
		ret = -1;
		goto open_file_err;
	}

	current = time(NULL);

	while (!feof(fp)) {
		memset(l, 0, 256);
		fgets(l, 255, fp);

		memset(s1, 0, 16);
		memset(s2, 0, 64);
		memset(s3, 0, 64);

		sscanf(l, "%s\t%s\t%s", s1, s2, s3);

		if (strlen(s1) == 0 || strlen(s1) > 16) {
			perror("Invalid vmname from timer list file");
			continue;
		}

		memset(arg.name, 0, sizeof(arg.name));
		strncpy(arg.name, s1, sizeof(s1));

		expire = strtoul(s2, NULL, 10);
		if (expire == 0 || errno == ERANGE) {
			perror("Invalid expire from timer list file");
			continue;
		}

		record = strtoul(s3, NULL, 10);
		if (record == 0 || errno == ERANGE) {
			perror("Invalid record time from timer list file");
			continue;
		}

		if (current == (time_t) -1)
			current = record;

		if (expire > current)
			expire -= current;
		else
			expire = 1;

		if (acrnd_add_work(acrnd_vm_timer_func, &arg, expire))
			ret = -1;
	}

	fclose(fp);

 open_file_err:
	pthread_mutex_unlock(&timer_file_mutex);
	return ret;
}

#define ACRND_LOG_FMT	"/opt/acrn/%s.log"

static void acrnd_run_vm(char *name)
{
	char log_path[128] = {};

	snprintf(log_path, sizeof(log_path) -1, ACRND_LOG_FMT, name);
	unlink(log_path);
	stdin = freopen(log_path, "w+", stdin);
	stdout = freopen(log_path, "w+", stdout);
	stderr = freopen(log_path, "w+", stderr);
	fflush(stdin);
	fflush(stdout);
	fflush(stderr);

	start_vm(name);
	printf("%s exited!\n", name);
	exit(0);
}

static int active_all_vms(void)
{
	struct vmmngr_struct *vm;
	int ret = 0;
	pid_t pid;

	vmmngr_update();

	LIST_FOREACH(vm, &vmmngr_head, list) {
		switch (vm->state) {
		case VM_CREATED:
			pid = fork();
			if (!pid)
				acrnd_run_vm(vm->name);
			break;
		case VM_PAUSED:
			ret += resume_vm(vm->name);
			break;
		default:
			pdebug();
		}
	}

	return ret ? -1 : 0;
}

#define SOS_LCS_SOCK		"sos-lcs"
#define DEFAULT_TIMEOUT	2U
#define SOS_ADVANCE_WKUP	10U	/* WKUP SOS 10 sec in advance */
#define ACRND_NAME		"acrnd"
static int acrnd_fd = -1;

unsigned get_sos_wakeup_reason(void)
{
	int client_fd, ret = 0;
	struct req_wakeup_reason req;
	struct ack_wakeup_reason ack;

	client_fd = mngr_open_un(SOS_LCS_SOCK, MNGR_CLIENT);
	if (client_fd <= 0) {
		fprintf(stderr, "Failed to open the socket(%s) to query the "
				"reason for the wake-up", SOS_LCS_SOCK);
		goto EXIT;
	}

	req.msg.magic = MNGR_MSG_MAGIC;
	req.msg.msgid = WAKEUP_REASON;
	req.msg.timestamp = time(NULL);
	req.msg.len = sizeof(struct req_wakeup_reason);

	if (mngr_send_msg(client_fd, (void *)&req, (void *)&ack, sizeof(ack),
			  DEFAULT_TIMEOUT))
		fprintf(stderr, "Failed to get wakeup_reason from SOS, err(%d)\n", ret);
	else
		ret = ack.reason;

	mngr_close(client_fd);
 EXIT:
	return ret;
}

static void handle_timer_req(struct mngr_msg *msg, int client_fd, void *param)
{
	struct req_acrnd_timer *req = (void *)msg;
	struct ack_acrnd_timer ack;
	struct vmmngr_struct *vm;
	struct work_arg arg = {};

	ack.msg.msgid = req->msg.msgid;
	ack.msg.len = sizeof(ack);
	ack.msg.timestamp = req->msg.timestamp;
	ack.err = -1;

	vmmngr_update();
	vm = vmmngr_find(req->name);
	if (!vm) {
		pdebug();
		goto reply_ack;
	}

	strncpy(arg.name, req->name, sizeof(arg.name) - 1);

	if (acrnd_add_work(acrnd_vm_timer_func, &arg, req->t)) {
		pdebug();
		goto reply_ack;
	}

	ack.err = 0;
 reply_ack:
	if (client_fd > 0)
		mngr_send_msg(client_fd, (void *)&ack, NULL, 0, 0);
}

static int set_sos_timer(time_t due_time)
{
	int client_fd, ret;
	int retry = 1;
	struct req_rtc_timer req;
	struct ack_rtc_timer ack;

	client_fd = mngr_open_un(SOS_LCS_SOCK, MNGR_CLIENT);
	if (client_fd <= 0) {
		perror("Failed to open sock for to req wkup_reason");
		ret = client_fd;
		goto EXIT;
	}

	req.msg.magic = MNGR_MSG_MAGIC;
	req.msg.msgid = RTC_TIMER;
	req.msg.timestamp = time(NULL);
	req.msg.len = sizeof(struct req_rtc_timer);
	req.t = due_time;

 RETRY:
	ret =
	    mngr_send_msg(client_fd, (void *)&req, (void *)&ack, sizeof(ack),
			  DEFAULT_TIMEOUT);
	while (ret != 0 && retry < 5) {
		printf("Fail to set sos wakeup timer(err:%d), retry %d...\n",
		       ret, retry++);
		goto RETRY;
	}

	mngr_close(client_fd);
 EXIT:
	return ret;
}

static int store_timer_list(void)
{
	FILE *fp;
	struct acrnd_work *w;
	time_t sys_wakeup = 0;
	time_t current;
	int ret = 0;

	current = time(NULL);
	if (current == (time_t) - 1) {
		pdebug();
		return -1;
	}

	pthread_mutex_lock(&timer_file_mutex);
	fp = fopen(TIMER_LIST_FILE, "w+");
	if (!fp) {
		perror("Open timer list file");
		ret = -1;
		goto open_file_err;
	}
	pthread_mutex_lock(&work_mutex);
	LIST_FOREACH(w, &work_head, list) {
		if (w->func != acrnd_vm_timer_func)
			continue;
		if (!sys_wakeup)
			sys_wakeup = w->expire;
		if (w->expire < sys_wakeup)
			sys_wakeup = w->expire;
		fprintf(fp, "%s\t%lu\t%lu\n", w->arg.name, w->expire, current);
	}
	pthread_mutex_unlock(&work_mutex);

	/* If any timer is stored
	 * system must be awake at sys_wakeup */
	if (sys_wakeup) {
		if (sys_wakeup > SOS_ADVANCE_WKUP)
			sys_wakeup -= SOS_ADVANCE_WKUP;
		set_sos_timer(sys_wakeup);
	} else {
		unlink(TIMER_LIST_FILE);
	}

	fclose(fp);
 open_file_err:
	pthread_mutex_unlock(&timer_file_mutex);
	return ret;
}

static int check_vms_status(unsigned int status)
{
	struct vmmngr_struct *s;

	vmmngr_update();

	LIST_FOREACH(s, &vmmngr_head, list)
	    if (s->state != status && s->state != VM_CREATED)
		return -1;

	return 0;
}

static int _handle_acrnd_stop(unsigned int timeout)
{
	unsigned long t = timeout;

	/*Let ospm stopping UOSs */

	/* list and update the vm status */
	do {
		if (check_vms_status(VM_CREATED) == 0)
			return 0;
		sleep(1);
	}
	while (t--);

	return -1;
}

static void handle_acrnd_stop(struct mngr_msg *msg, int client_fd, void *param)
{
	struct req_acrnd_stop *req = (void *)msg;
	struct ack_acrnd_stop ack;

	ack.msg.msgid = req->msg.msgid;
	ack.msg.len = sizeof(ack);
	ack.msg.timestamp = req->msg.timestamp;
	ack.err = _handle_acrnd_stop(req->timeout);

	store_timer_list();

	if (client_fd > 0)
		mngr_send_msg(client_fd, (void *)&ack, NULL, 0, 0);
}

void handle_acrnd_resume(struct mngr_msg *msg, int client_fd, void *param)
{
	struct req_acrnd_resume *req = (void *)msg;
	struct ack_acrnd_resume ack;
	struct stat st;
	int wakeup_reason;

	ack.msg.msgid = req->msg.msgid;
	ack.msg.len = sizeof(ack);
	ack.msg.timestamp = req->msg.timestamp;
	ack.err = 0;

	/* Do we have a timer list file to load? */
	if (!stat(TIMER_LIST_FILE, &st))
		if (S_ISREG(st.st_mode)) {
			ack.err = load_timer_list();
			if (ack.err)
				pdebug();
			goto reply_ack;
		}

	/* acrnd get wakeup_reason from sos lcs */
	wakeup_reason = get_sos_wakeup_reason();

	if (wakeup_reason & CBC_WK_RSN_RTC) {
		/* do nothing, just wait the acrnd_work to expire */
		goto reply_ack;
	}

	ack.err = active_all_vms();

 reply_ack:
	unlink(TIMER_LIST_FILE);

	if (client_fd > 0)
		mngr_send_msg(client_fd, (void *)&ack, NULL, 0, 0);
}

static void handle_on_exit(void)
{
	store_timer_list();

	if (acrnd_fd > 0) {
		mngr_close(acrnd_fd);
		acrnd_fd = -1;
	}
}

int init_vm(void)
{
	unsigned int wakeup_reason;
	struct stat st;

	if (!stat(TIMER_LIST_FILE, &st))
		if (S_ISREG(st.st_mode))
			return load_timer_list();

	/* init all UOSs, according wakeup_reason */
	wakeup_reason = get_sos_wakeup_reason();

	if (wakeup_reason & CBC_WK_RSN_RTC)
		return load_timer_list();
	else {
		/* TODO: auto start UOSs */
		return active_all_vms();
	}
}

int main(int argc, char *argv[])
{
	/* create listening thread */
	acrnd_fd = mngr_open_un(ACRND_NAME, MNGR_SERVER);
	if (acrnd_fd < 0) {
		pdebug();
		return -1;
	}

	if (init_vm()) {
		pdebug();
		return -1;
	}

	unlink(TIMER_LIST_FILE);

	atexit(handle_on_exit);

	mngr_add_handler(acrnd_fd, ACRND_TIMER, handle_timer_req, NULL);
	mngr_add_handler(acrnd_fd, ACRND_STOP, handle_acrnd_stop, NULL);
	mngr_add_handler(acrnd_fd, ACRND_RESUME, handle_acrnd_resume, NULL);

	/* Last thing, run our timer works */
	while (1) {
		try_do_works();
		sleep(1);
	}

	return 0;
}
