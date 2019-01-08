/*
 * Copyright (C)2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <time.h>
#include <pthread.h>
#include <signal.h>
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

static pthread_mutex_t acrnd_stop_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int acrnd_stop_timeout;

static int sigterm = 0; /* Exit acrnd when recevied SIGTERM and stop all vms */
#define VMS_STOP_TIMEOUT 20 /* Wait VMS_STOP_TIMEOUT sec to stop all vms */

/* acrnd_add_work(), add a worker function.
 * @func, the worker function, will be called with work_mutex hold.
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

	printf("%s added, work->expire:%lx\n", __FUNCTION__, work->expire);
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

	pthread_mutex_lock(&work_mutex);
	list_foreach_safe(work, &work_head, list, twork) {
		if (current >= work->expire) {
			printf("Run expired work(%lu), current time(%lu)\n",
								work->expire, current);
			work->func(&work->arg);
			LIST_REMOVE(work, list);
			free(work);
		}
	}
	pthread_mutex_unlock(&work_mutex);
}

static void acrnd_run_vm(char *name);
unsigned get_sos_wakeup_reason(void);

/* Timer callback to run/resume VM.
 * Will be called with work_mutex hold
 */
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
	case VM_SUSPENDED:
		resume_vm(arg->name, CBC_WK_RSN_RTC);
		break;
	default:
		pdebug();
	}
}

static pthread_mutex_t timer_file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* load/store_timer_list to file to keep timers if SOS poweroff */
static int load_timer_list(void)
{
	FILE *fp;
	struct work_arg arg = {};
	time_t expire, current;
	char l[256];
	int i, ret = 0;

	pthread_mutex_lock(&timer_file_mutex);

	fp = fopen(ACRN_CONF_TIMER_LIST, "r");
	if (!fp) {
		perror("Open timer list file");
		ret = -1;
		goto open_file_err;
	}

	while (!feof(fp)) {
		memset(l, 0, 256);

		/* Ignore null line */
		if (!fgets(l, 255, fp)) {
			continue;
		}

		/* get vmname from the string "l", which has "[vmname]\t[expire]" */
		for (i = 0; i < sizeof(arg.name); i++) {
			if (l[i] == '\t') {
				arg.name[i] = '\0';
				break;
			}
			arg.name[i] = l[i];
		}

		/* can't found vmname in the string "l" or vmname is truncated */
		if (i == 0 || i == sizeof(arg.name)) {
			fprintf(stderr, "Invalid vmname %s from timer list file\n", arg.name);
			continue;
		}

		/* get expire from the string "l" */
		expire = strtoul(&l[i + 1], NULL, 10);
		if (expire == 0 || errno == ERANGE) {
			perror("Invalid expire from timer list file");
			continue;
		}

		current = time(NULL);
		if (expire > current)
			expire -= current;
		else
			expire = 1;

		ret = acrnd_add_work(acrnd_vm_timer_func, &arg, expire);
		if (ret != 0) {
			fprintf(stderr, "Failed to add vm timer, errno %d", ret);
		} else {
			printf("vm %s will be activated at %ld seconds later\n", arg.name, expire);
		}
	}

	fclose(fp);

 open_file_err:
	pthread_mutex_unlock(&timer_file_mutex);
	return ret;
}

static int logfile = 1;

static void acrnd_run_vm(char *name)
{
	/*If do not use logfile, then output to stdout,
	 so that it can be redirected to journal by systemd */
	if (logfile) {
		stdin = freopen("/dev/null", "r+", stdin);
		stdout = freopen("/dev/null", "r+", stdout);
		stderr = freopen("/dev/null", "r+", stderr);
		fflush(stdin);
		fflush(stdout);
		fflush(stderr);
	}

	start_vm(name);
	printf("%s exited!\n", name);
	exit(0);
}

static int active_all_vms(void)
{
	struct vmmngr_struct *vm;
	int ret = 0;
	pid_t pid;
	unsigned reason;

	vmmngr_update();

	LIST_FOREACH(vm, &vmmngr_head, list) {
		switch (vm->state) {
		case VM_CREATED:
			pid = fork();
			if (!pid)
				acrnd_run_vm(vm->name);
			break;
		case VM_SUSPENDED:
			reason = get_sos_wakeup_reason();
			ret += resume_vm(vm->name, reason);
			break;
		default:
			pdebug();
		}
	}

	return ret ? -1 : 0;
}

static void stop_all_vms(void)
{
	struct vmmngr_struct *vm;
	int err;

	vmmngr_update();

	LIST_FOREACH(vm, &vmmngr_head, list) {
		err = stop_vm(vm->name);
		if (err != 0) {
			fprintf(stderr, "Fail to send stop cmd to vm %s\n", vm->name);
		} else {
			printf("Send stop cmd to vm %s successfully\n", vm->name);
		}
	}
}

static int wakeup_suspended_vms(unsigned wakeup_reason)
{
	struct vmmngr_struct *vm;
	int ret = 0;

	vmmngr_update();

	LIST_FOREACH(vm, &vmmngr_head, list) {
		if (vm->state == VM_SUSPENDED)
			ret += resume_vm(vm->name, wakeup_reason);
	}

	return ret ? -1 : 0;
}

#define SOS_LCS_SOCK		"sos-lcs"
#define DEFAULT_TIMEOUT	2U
#define ACRND_NAME		"acrnd"
static int acrnd_fd = -1;

unsigned get_sos_wakeup_reason(void)
{
	int client_fd, ret = 0;
	struct mngr_msg req;
	struct mngr_msg ack;

	client_fd = mngr_open_un(SOS_LCS_SOCK, MNGR_CLIENT);
	if (client_fd <= 0) {
		fprintf(stderr, "Failed to open the socket(%s) to query the "
				"reason for the wake-up", SOS_LCS_SOCK);
		goto EXIT;
	}

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = WAKEUP_REASON;
	req.timestamp = time(NULL);

	if (mngr_send_msg(client_fd, &req, &ack, DEFAULT_TIMEOUT) <= 0)
		fprintf(stderr, "Failed to get wakeup_reason from SOS, err(%d)\n", ret);
	else
		ret = ack.data.reason;

	mngr_close(client_fd);
 EXIT:
	return ret;
}

static void handle_timer_req(struct mngr_msg *msg, int client_fd, void *param)
{
	struct mngr_msg ack;
	struct vmmngr_struct *vm;
	struct work_arg arg = {};

	ack.msgid = msg->msgid;
	ack.timestamp = msg->timestamp;
	ack.data.err = -1;

	vmmngr_update();
	vm = vmmngr_find(msg->data.acrnd_timer.name);
	if (!vm) {
		pdebug();
		goto reply_ack;
	}

	strncpy(arg.name, msg->data.acrnd_timer.name, sizeof(arg.name) - 1);
	if (sizeof(arg.name) - 1 < strnlen(msg->data.acrnd_timer.name, VMNAME_LEN)) {
		perror("timer name was truncated\n");
		goto reply_ack;
	}

	if (acrnd_add_work(acrnd_vm_timer_func, &arg, msg->data.acrnd_timer.t)) {
		pdebug();
		goto reply_ack;
	}

	ack.data.err = 0;
 reply_ack:
	if (client_fd > 0)
		mngr_send_msg(client_fd, &ack, NULL, 0);
}

static int set_sos_timer(time_t due_time)
{
	int client_fd, ret;
	int retry = 1;
	struct mngr_msg req;
	struct mngr_msg ack;

	client_fd = mngr_open_un(SOS_LCS_SOCK, MNGR_CLIENT);
	if (client_fd <= 0) {
		perror("Failed to open sock for to req wkup_reason");
		ret = client_fd;
		goto EXIT;
	}

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = RTC_TIMER;
	req.timestamp = time(NULL);
	req.data.rtc_timer.t = due_time;

 RETRY:
	ret =
	    mngr_send_msg(client_fd, &req, &ack, DEFAULT_TIMEOUT);
	while (ret <= 0 && retry < 5) {
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
	struct acrnd_work *w, *twork;
	time_t sys_wakeup = 0;
	int ret = 0;

	pthread_mutex_lock(&timer_file_mutex);
	fp = fopen(ACRN_CONF_TIMER_LIST, "w+");
	if (!fp) {
		perror("Open timer list file");
		ret = -1;
		goto open_file_err;
	}
	pthread_mutex_lock(&work_mutex);
	list_foreach_safe(w, &work_head, list, twork) {
		if (w->func != acrnd_vm_timer_func)
			continue;
		if (!sys_wakeup)
			sys_wakeup = w->expire;
		if (w->expire < sys_wakeup)
			sys_wakeup = w->expire;
		fprintf(fp, "%s\t%lu\n", w->arg.name, w->expire);

		/* remove work from list after it's saved onto fs */
		LIST_REMOVE(w, list);
		free(w);
	}
	pthread_mutex_unlock(&work_mutex);

	/* If any timer is stored
	 * system must be awake at sys_wakeup */
	if (sys_wakeup) {
		set_sos_timer(sys_wakeup);
	} else {
		unlink(ACRN_CONF_TIMER_LIST);
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

static int wait_for_stop(unsigned int timeout)
{
	unsigned long t = timeout;

	/*Let ospm stopping UOSs */

	/* list and update the vm status */
	do {
		printf("Waiting %lu seconds for all vms enter S3/S5 state\n", t);

		if (check_vms_status(VM_CREATED) == 0) {
			printf("All vms have entered S5 state successfully\n");
			return SHUTDOWN;
		}

		if (check_vms_status(VM_SUSPENDED) == 0) {
			printf("All vms have entered S3 state successfully\n");
			return SUSPEND;
		}

		sleep(1);
	}
	while (t--);

	return -1;
}

static void* notify_stop_state(void *arg)
{
	int lcs_fd;
	int rc;
	struct mngr_msg req;

	req.magic = MNGR_MSG_MAGIC;

	rc = wait_for_stop(acrnd_stop_timeout);
	if (rc < 0) {
		fprintf(stderr, "Timeout(%u sec) to wait all vms enter S3/S5\n", acrnd_stop_timeout);
		req.msgid = SUSPEND;
		req.data.err = -1;
	} else {
		req.msgid = rc;
		req.data.err = 0;
	}

	store_timer_list();

	lcs_fd = mngr_open_un(SOS_LCS_SOCK, MNGR_CLIENT);
	if (lcs_fd < 0) {
		fprintf(stderr, "cannot open sos-lcs.socket\n");
		goto exit;
	}

	mngr_send_msg(lcs_fd, &req, NULL, 0);
	mngr_close(lcs_fd);
exit:
	pthread_mutex_unlock(&acrnd_stop_mutex);
	return NULL;
}

static void _handle_acrnd_stop(unsigned int timeout)
{
	int rc;
	pthread_t tid;
	pthread_attr_t attr;

	/*
	 * Only one acrnd stop thread at a time
	 * if failed to lock the acrnd_stop_mutex, then return directly
	 * if creating thread success, then unlock in the thread exit
	 * if failed to create thread, then unlock immediately.
	 */
	if (pthread_mutex_trylock(&acrnd_stop_mutex) == 0) {

		acrnd_stop_timeout = timeout;

		/*
		 * Due to acrnd only has one main thread, and acrnd stop flow
		 * probably blocks main thread, so a detached thread is created
		 * to avoid this.
		 */
		rc = pthread_attr_init(&attr);
		if (rc < 0)
			goto fail_init;
		rc = pthread_attr_setdetachstate(&attr,
				PTHREAD_CREATE_DETACHED);
		if (rc < 0)
			goto fail;
		rc = pthread_create(&tid, &attr, notify_stop_state, NULL);
		if (rc < 0)
			goto fail;

		pthread_attr_destroy(&attr);
	}

	return;

fail:
	pthread_attr_destroy(&attr);

fail_init:
	pthread_mutex_unlock(&acrnd_stop_mutex);
	fprintf(stderr, "Failed to invoke handle_acrnd_stop \n");
}

static void handle_acrnd_stop(struct mngr_msg *msg, int client_fd, void *param)
{
	struct mngr_msg ack;

	ack.msgid = msg->msgid;
	ack.timestamp = msg->timestamp;
	ack.data.err = 0;
	if (client_fd > 0)
		mngr_send_msg(client_fd, &ack, NULL, 0);

	_handle_acrnd_stop(msg->data.acrnd_stop.timeout);
}

void handle_acrnd_resume(struct mngr_msg *msg, int client_fd, void *param)
{
	struct mngr_msg ack;
	struct stat st;
	int wakeup_reason;

	ack.msgid = msg->msgid;
	ack.timestamp = msg->timestamp;
	ack.data.err = -1;

	/* acrnd get wakeup_reason from sos lcs */
	wakeup_reason = get_sos_wakeup_reason();

	if (wakeup_reason & CBC_WK_RSN_RTC) {
		printf("Resumed UOS, by RTC timer, reason(%x)!\n", wakeup_reason);
		/* wakeup by RTC timer */
		if (!stat(ACRN_CONF_TIMER_LIST, &st)
			&& S_ISREG(st.st_mode)) {
			ack.data.err = load_timer_list();
			if (ack.data.err == 0) {
				printf("Resumed UOS by RTC timer, try do works!\n");
				/* load timers successfully */
				try_do_works();
				goto reply_ack;
			}
		}

		perror("Error to load timers, wakeup all VMs");
		ack.data.err = wakeup_suspended_vms(wakeup_reason);	
	} else {
		printf("Resumed UOS, by ignition button\n");
		ack.data.err = wakeup_suspended_vms(wakeup_reason);
	}

reply_ack:
	unlink(ACRN_CONF_TIMER_LIST);

	if (client_fd > 0)
		mngr_send_msg(client_fd, &ack, NULL, 0);
}

static void handle_on_exit(void)
{
	printf("Exiting from acrnd\n");
	store_timer_list();

	if (acrnd_fd > 0) {
		mngr_close(acrnd_fd);
		acrnd_fd = -1;
	}
}

int init_vm(void)
{
	unsigned int wakeup_reason;
	int ret;

	/* init all UOSs, according wakeup_reason */
	wakeup_reason = get_sos_wakeup_reason();

	if (wakeup_reason & CBC_WK_RSN_RTC) {
		printf("Loading timer list to set vms wakeup time\n");
		ret = load_timer_list();
		if (ret == 0) {
			printf("Successfully activate vms from timer list\n");
			return 0;
		} else {
			printf("Error happens when load timer list, errno %d\n", ret);
		}
	}

	/* TODO: auto start UOSs */
	printf("Activating all vms\n");
	return active_all_vms();
}

static void sigterm_handler(int signo)
{
	printf("Received signal %d, Will stop all the vms within %d sec\n", signo, VMS_STOP_TIMEOUT);
	sigterm = 1;
}

static const char optString[] = "t";

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, optString)) != -1) {
		switch (opt) {
		case 't':
			 logfile = 0;
			 break;
		default:
			 printf("Ingrone unknown opt: %c\n", opt);
		}
	}

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

	unlink(ACRN_CONF_TIMER_LIST);

	atexit(handle_on_exit);

	if (signal(SIGTERM, sigterm_handler) == SIG_ERR) {
		fprintf(stderr, "Can not catch signal SIGTERM(%d), err: %s\n", SIGTERM, strerror(errno));
	}

	mngr_add_handler(acrnd_fd, ACRND_TIMER, handle_timer_req, NULL);
	mngr_add_handler(acrnd_fd, ACRND_STOP, handle_acrnd_stop, NULL);
	mngr_add_handler(acrnd_fd, ACRND_RESUME, handle_acrnd_resume, NULL);

	/* Last thing, run our timer works */
	while (!sigterm) {
		try_do_works();
		sleep(1);
	}

	/*
	 * Try to stop all the vms when receiving SIGTERM within VMS_STOP_TIMEOUT sec
	 * gracefully. acrnd will exit after waiting maximal VMS_STOP_TIMEOUT sec.
	 * System will kill all other vms which can not be stopped within VMS_STOP_TIMEOUT sec.
	 */
	stop_all_vms();
	wait_for_stop(VMS_STOP_TIMEOUT);

	return 0;
}
