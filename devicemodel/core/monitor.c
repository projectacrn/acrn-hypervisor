/*
 * Project Acrn
 * Acrn-dm-monitor
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *
 * Author: TaoYuhong <yuhong.tao@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <unistd.h>
#include <pthread.h>
#include "dm.h"
#include "dm_string.h"
#include "monitor.h"
#include "acrn_mngr.h"
#include "pm.h"
#include "vmmapi.h"

#define INTR_STORM_MONITOR_PERIOD	10 /* 10 seconds */
#define INTR_STORM_THRESHOLD	100000 /* 10K times per second */

#define DELAY_INTR_TIME	1 /* 1ms */
#define DELAY_DURATION	100000 /* 100ms of total duration for delay intr */
#define TIME_TO_CHECK_AGAIN	2 /* 2seconds */

struct intr_monitor_setting_t {
	bool enable;
	uint32_t threshold;    /* intr count in probe_period when intr storm happens */
	uint32_t probe_period;  /* seconds: the period to probe intr data */
	uint32_t delay_time;      /* ms: the time to delay each intr injection */
	uint32_t delay_duration;  /* us: the delay duration, after it, intr injection restore to normal */
};

union intr_monitor_t {
	struct acrn_intr_monitor monitor;
	char reserved[4096];
} __aligned(4096);

static union intr_monitor_t intr_data;
static uint64_t intr_cnt_buf[MAX_PTDEV_NUM * 2];
static pthread_t intr_storm_monitor_pid;

static struct intr_monitor_setting_t intr_monitor_setting = {
	.enable = false,
};

/* switch macro, just open in debug */
/* #define INTR_MONITOR_DBG */

#ifdef INTR_MONITOR_DBG
static FILE * dbg_file;
#define DPRINTF(format, args...) \
do { fprintf(dbg_file, format, args); fflush(dbg_file); } while (0)

/* this is a debug function */
static void write_intr_data_to_file(const struct acrn_intr_monitor *hdr)
{
	static int wr_cnt;
	int j;

	wr_cnt++;
	fprintf(dbg_file, "\n==%d time devs=%d==\n", wr_cnt, hdr->buf_cnt / 2);
	fprintf(dbg_file, "IRQ\t\tCount\n");

	for (j = 0; j < hdr->buf_cnt; j += 2) {
		if (hdr->buffer[j + 1] != 0) {
			fprintf(dbg_file, "%ld\t\t%ld\n", hdr->buffer[j], hdr->buffer[j + 1]);
		}
	}

	fflush(dbg_file);
}
#else
#define DPRINTF(format, arg...)
#endif

static void *intr_storm_monitor_thread(void *arg)
{
	struct vmctx *ctx = (struct vmctx *)arg;
	struct acrn_intr_monitor *hdr = &intr_data.monitor;
	uint64_t delta = 0UL;
	int ret, i;

#ifdef INTR_MONITOR_DBG
	dbg_file = fopen("/tmp/intr_log", "w+");
#endif
	sleep(intr_monitor_setting.probe_period);

	/* first to get interrupt data */
	hdr->cmd = INTR_CMD_GET_DATA;
	hdr->buf_cnt = MAX_PTDEV_NUM * 2;
	memset(hdr->buffer, 0, sizeof(uint64_t) * hdr->buf_cnt);

	ret = vm_intr_monitor(ctx, hdr);
	if (ret) {
		DPRINTF("first get intr data failed, ret: %d\n", ret);
		intr_storm_monitor_pid = 0;
		return NULL;
	}

	while (1) {
#ifdef INTR_MONITOR_DBG
		write_intr_data_to_file(hdr);
#endif
		memcpy(intr_cnt_buf, hdr->buffer, sizeof(uint64_t) * hdr->buf_cnt);
		sleep(intr_monitor_setting.probe_period);

		/* next time to get interrupt data */
		memset(hdr->buffer, 0, sizeof(uint64_t) * hdr->buf_cnt);
		ret = vm_intr_monitor(ctx, hdr);
		if (ret) {
			DPRINTF("next get intr data failed, ret: %d\n", ret);
			intr_storm_monitor_pid = 0;
			break;
		}

		/*
		 * calc the delta of the two times count of interrupt;
		 * compare the IRQ num first, if not same just drop it,
		 * for it just happens rarelly when devices dynamically
		 * allocation in SOS or UOS, it can be calc next time
		 */
		for (i = 0; i < hdr->buf_cnt; i += 2) {
			if (hdr->buffer[i] != intr_cnt_buf[i])
				continue;

			/* avoid delta overflow */
			if (hdr->buffer[i + 1] < intr_cnt_buf[i + 1])
				continue;

			delta = hdr->buffer[i + 1] - intr_cnt_buf[i + 1];
			if (delta > intr_monitor_setting.threshold) {
#ifdef INTR_MONITOR_DBG
				write_intr_data_to_file(hdr);
#endif
				break;
			}
		}

		/* storm detected, handle the intr abnormal status */
		if (i < hdr->buf_cnt) {
			DPRINTF("irq=%ld, delta=%ld\n", intr_cnt_buf[i], delta);

			hdr->cmd = INTR_CMD_DELAY_INT;
			hdr->buffer[0] = intr_monitor_setting.delay_time;
			vm_intr_monitor(ctx, hdr);
			usleep(intr_monitor_setting.delay_duration); /* sleep-delay intr */
			hdr->buffer[0] = 0; /* cancel to delay intr */
			vm_intr_monitor(ctx, hdr);

			sleep(TIME_TO_CHECK_AGAIN); /* time to get data again */
			hdr->cmd = INTR_CMD_GET_DATA;
			hdr->buf_cnt = MAX_PTDEV_NUM * 2;
			memset(hdr->buffer, 0, sizeof(uint64_t) * hdr->buf_cnt);
			vm_intr_monitor(ctx, hdr);
		}
	}

	return NULL;
}

static void start_intr_storm_monitor(struct vmctx *ctx)
{
	if (intr_monitor_setting.enable) {
		int ret = pthread_create(&intr_storm_monitor_pid, NULL, intr_storm_monitor_thread, ctx);
		if (ret) {
			printf("failed %s %d\n", __func__, __LINE__);
			intr_storm_monitor_pid = 0;
		}
		pthread_setname_np(intr_storm_monitor_pid, "storm_monitor");

		printf("start monitor interrupt data...\n");
	}
}

static void stop_intr_storm_monitor(void)
{
	if (intr_storm_monitor_pid) {
		void *ret;

		pthread_cancel(intr_storm_monitor_pid);
		pthread_join(intr_storm_monitor_pid, &ret);
		intr_storm_monitor_pid = 0;
	}
}

/*
.* interrupt monitor setting params, current interrupt mitigation will delay UOS's
.* pass-through devices' interrupt injection, the settings input from acrn-dm:
.* params:
.* threshold: each intr count/second when intr storm happens;
.* probe_period: seconds -- the period to probe intr data;
.* delay_time: ms -- the time to delay each intr injection;
 * delay_duration; us -- the delay duration, after it, intr injection restore to normal
.*/
int acrn_parse_intr_monitor(const char *opt)
{
	uint32_t threshold, period, delay, duration;
	char *cp;

	if((!dm_strtoui(opt, &cp, 10, &threshold) && *cp == ',') &&
		(!dm_strtoui(cp + 1, &cp, 10, &period) && *cp == ',') &&
		(!dm_strtoui(cp + 1, &cp, 10, &delay) && *cp == ',') &&
		(!dm_strtoui(cp + 1, &cp, 10, &duration))) {
		printf("interrupt storm monitor params: %d, %d, %d, %d\n", threshold, period, delay, duration);
	} else {
		printf("%s: not correct, it should be like: --intr_monitor 10000,10,1,100, please check!\n", opt);
		return -1;
	}

	intr_monitor_setting.enable = true;
	intr_monitor_setting.threshold = threshold * period;
	intr_monitor_setting.probe_period = period;
	intr_monitor_setting.delay_time = delay;
	intr_monitor_setting.delay_duration = duration * 1000;

	return 0;
}


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
	struct mngr_msg req;
	struct mngr_msg ack;
	int ret;

	acrnd_fd = mngr_open_un("acrnd", MNGR_CLIENT);
	if (acrnd_fd < 0) {
		return -1;
	}

	req.magic = MNGR_MSG_MAGIC;
	req.msgid = ACRND_TIMER;
	req.timestamp = time(NULL);

	req.data.rtc_timer.t = t;
	strncpy(req.data.rtc_timer.vmname, vmname,
			sizeof(req.data.rtc_timer.vmname));

	memset(&ack, 0, sizeof(struct mngr_msg));
	ret = mngr_send_msg(acrnd_fd, &req, &ack, 2);
	mngr_close(acrnd_fd);
	if (ret != sizeof(ack)) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		return -1;
	}

	return ack.data.err;
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

#define DEFINE_HANDLER(name, func)				\
static void name(struct mngr_msg *msg, int client_fd, void *param)	\
{									\
	struct mngr_msg ack;					\
	struct vm_ops *ops;					\
								\
	int ret = 0;						\
	int count = 0;						\
								\
	ack.magic = MNGR_MSG_MAGIC;				\
	ack.msgid = msg->msgid;					\
	ack.timestamp = msg->timestamp;				\
								\
	LIST_FOREACH(ops, &vm_ops_head, list) {			\
		if (ops->ops->func) {				\
			ret += ops->ops->func(ops->arg);	\
			count++;				\
		}						\
	}							\
								\
	if (!count) {						\
		ack.data.err = -1;					\
		fprintf(stderr, "No handler for id:%u\r\n", msg->msgid);	\
	} else									\
		ack.data.err = ret;							\
										\
	mngr_send_msg(client_fd, &ack, NULL, ACK_TIMEOUT);		\
}

DEFINE_HANDLER(handle_stop, stop);
DEFINE_HANDLER(handle_suspend, suspend);
DEFINE_HANDLER(handle_pause, pause);
DEFINE_HANDLER(handle_continue, unpause);

static void handle_resume(struct mngr_msg *msg, int client_fd, void *param)
{
	struct mngr_msg ack;
	struct vm_ops *ops;
	int ret = 0;
	int count = 0;

	ack.magic = MNGR_MSG_MAGIC;
	ack.msgid = msg->msgid;
	ack.timestamp = msg->timestamp;

	wakeup_reason = msg->data.reason;

	LIST_FOREACH(ops, &vm_ops_head, list) {
		if (ops->ops->resume) {
			ret += ops->ops->resume(ops->arg);
			count++;
		}
	}

	if (!count) {
		ack.data.err = -1;
		fprintf(stderr, "No handler for id:%u\r\n", msg->msgid);
	} else
		ack.data.err = ret;

	mngr_send_msg(client_fd, &ack, NULL, ACK_TIMEOUT);
}

static void handle_query(struct mngr_msg *msg, int client_fd, void *param)
{
	struct mngr_msg ack;
	struct vm_ops *ops;

	ack.magic = MNGR_MSG_MAGIC;
	ack.msgid = msg->msgid;
	ack.timestamp = msg->timestamp;
	ack.data.state = -1;

	LIST_FOREACH(ops, &vm_ops_head, list) {
		if (ops->ops->query) {
			ack.data.state = ops->ops->query(ops->arg);
			break;
		}
	}

	mngr_send_msg(client_fd, &ack, NULL, ACK_TIMEOUT);
}

static struct monitor_vm_ops pmc_ops = {
	.stop       = NULL,
	.resume     = vm_monitor_resume,
	.suspend    = NULL,
	.pause      = NULL,
	.unpause    = NULL,
	.query      = vm_monitor_query,
};

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

	monitor_register_vm_ops(&pmc_ops, ctx, "PMC_VM_OPs");

	start_intr_storm_monitor(ctx);

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

	stop_intr_storm_monitor();
}
