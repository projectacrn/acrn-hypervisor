/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#include "acrntrace.h"

#define TIMER_ID	(128)
static uint32_t timeout = 0;

static int exiting = 0;

/* for opt */
static uint64_t period = 10000;
static const char optString[] = "i:hct:";
static const char dev_prefix[] = "acrn_trace_";

static uint32_t flags;
static char trace_file_dir[TRACE_FILE_DIR_LEN];

static reader_struct *reader;
static int dev_cnt = 0; /* Count of /dev/acrn_trace_xxx devices */

static void display_usage(void)
{
	printf("acrntrace - tool to collect ACRN trace data\n"
	       "[Usage] acrntrace [-i period] [-t max_time] [-ch]\n\n"
	       "[Options]\n"
	       "\t-h: print this message\n"
	       "\t-i: period_in_ms: specify polling interval [1-999]\n"
	       "\t-t: max time to capture trace data (in second)\n"
	       "\t-c: clear the buffered old data\n");
}

static void timer_handler(union sigval sv)
{
	exiting = 1;
	exit(0);
}

static int init_timer(int timeout)
{
	struct sigevent event;
	struct itimerspec it;
	timer_t tm_id;
	int err;

	memset(&event, 0, sizeof(struct sigevent));

	event.sigev_value.sival_int = TIMER_ID;
	event.sigev_notify = SIGEV_THREAD;
	event.sigev_notify_function = timer_handler;

	err = timer_create(CLOCK_MONOTONIC, &event, &tm_id);
	if (err < 0) {
		pr_err("Error to create timer, errno(%d)\n", err);
		return err;
	}

	it.it_interval.tv_sec = timeout;
	it.it_interval.tv_nsec = 0;
	it.it_value.tv_sec = timeout;
	it.it_value.tv_nsec = 0;

	err = timer_settime(tm_id, 0, &it, NULL);
	if (err < 0) {
		pr_err("Error to set timer, errno(%d)\n", err);
		return err;
	}

	pr_info("Capture trace data for about %ds and exit\n", timeout);
	return 0;
}

static int parse_opt(int argc, char *argv[])
{
	int opt, ret;

	while ((opt = getopt(argc, argv, optString)) != -1) {
		switch (opt) {
		case 'i':
			ret = strtol(optarg, NULL, 10);
			if (ret <= 0 || ret >=1000) {
				pr_err("'-i' require integer between [1-999]\n");
				return -EINVAL;
			}
			period = ret * 1000;
			pr_dbg("Period is %lu\n", period);
			break;
		case 't':
			ret = strtol(optarg, NULL, 10);
			if (ret <= 0) {
				pr_err("'-t' require integer greater than 0\n");
				return -EINVAL;
			}
			timeout = ret;
			pr_dbg("Capture trace data for at most %ds\n", ret);
			break;
		case 'c':
			flags |= FLAG_CLEAR_BUF;
			break;
		case 'h':
			display_usage();
			return -EINVAL;
		default:
			/* Undefined operation. */
			display_usage();
			return -EINVAL;
		}
	};
	return 0;
}

static int get_dev_cnt(void)
{
	struct dirent *pdir;
	int cnt = 0;
	char *ret;
	DIR *dir;

	dir = opendir("/dev");
	if (!dir) {
		printf("Error opening /dev: %s\n", strerror(errno));
		return -1;
	}

	while ((pdir = readdir(dir)) != NULL) {
		ret = strstr(pdir->d_name, dev_prefix);
		if (ret)
			cnt++;
	}

	closedir(dir);

	return cnt;
}

static int create_trace_file_dir(char *dir)
{
	int err = 0, ret;
	char time_str[TIME_STR_LEN + 1];
	time_t timep;
	struct tm *p;
	struct stat st;

	time(&timep);
	p = localtime(&timep);
	if (p) {
		ret = snprintf(time_str, TIME_STR_LEN + 1,
			       "%04d%02d%02d-%02d%02d%02d",
			       (1900 + p->tm_year), (1 + p->tm_mon),
			       p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
		if (ret > TIME_STR_LEN)
			printf("WARN: time_str may be truncated\n");
	} else {
		if (snprintf(time_str, TIME_STR_LEN, "00000000-000000") >= TIME_STR_LEN)
			printf("WARN: time_str is truncated\n\n");
	}

	pr_info("start tracing at %s\n", time_str);

	if (stat(TRACE_FILE_ROOT, &st)) {
		err = mkdir(TRACE_FILE_ROOT, 0644);
		if (err) {
			pr_err("Fail to create dir %s, Error: %s\n",
				TRACE_FILE_ROOT, strerror(errno));
			return -1;
		}
	}

	ret = snprintf(dir, TRACE_FILE_DIR_LEN, "%s%s",
		       TRACE_FILE_ROOT, time_str);
	if (ret >= TRACE_FILE_DIR_LEN)
		printf("WARN: trace file dir name is truncated\n");
	if (stat(dir, &st)) {
		err = mkdir(dir, 0644);
		if (err) {
			pr_err("Fail to create dir %s, Error: %s\n",
				dir, strerror(errno));
			return -1;
		}
	}

	pr_dbg("dir %s creted\n", dir);

	return err;
}

/* function executed in each consumer thread */
static void reader_fn(param_t * param)
{
	int ret;
	int fd = param->trace_fd;
	shared_buf_t *sbuf = param->sbuf;

	pr_dbg("reader thread[%lu] created for FILE*[0x%p]\n",
	       pthread_self(), fp);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* Clear the old data in sbuf */
	if (flags & FLAG_CLEAR_BUF)
		sbuf_clear_buffered(sbuf);

	while (1) {
		do {
			ret = sbuf_write(fd, sbuf);
		} while (ret > 0);

		usleep(period);
	}
}

static int create_reader(reader_struct * reader, uint32_t dev_id)
{
	char trace_file_name[TRACE_FILE_NAME_LEN];

	if (snprintf(reader->dev_name, DEV_PATH_LEN, "/dev/%s%u", dev_prefix, dev_id)
			>= DEV_PATH_LEN)
		printf("WARN: device name is truncated\n");

	reader->param.devid = dev_id;

	reader->dev_fd = open(reader->dev_name, O_RDWR);
	if (reader->dev_fd < 0) {
		pr_err("Failed to open %s, err %d\n", reader->dev_name, errno);
		reader->dev_fd = 0;
		return -1;
	}

	reader->param.sbuf = mmap(NULL, MMAP_SIZE,
				  PROT_READ | PROT_WRITE,
				  MAP_SHARED, reader->dev_fd, 0);
	if (reader->param.sbuf == MAP_FAILED) {
		pr_err("mmap failed for %s, errno %d\n", reader->dev_name, errno);
		reader->param.sbuf = NULL;
		return -2;
	}

	pr_dbg("sbuf[%d]:\nmagic_num: %lx\nele_num: %u\n ele_size: %u\n",
	       dev_id, reader->param.sbuf->magic, reader->param.sbuf->ele_num,
	       reader->param.sbuf->ele_size);

	if(snprintf(trace_file_name, TRACE_FILE_NAME_LEN, "%s/%d", trace_file_dir,
		 dev_id) >= TRACE_FILE_NAME_LEN)
		printf("WARN: trace file name is truncated\n");
	reader->param.trace_fd = open(trace_file_name,
					O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (!reader->param.trace_fd) {
		pr_err("Failed to open %s, err %d\n", trace_file_name, errno);
		return -3;
	}

	pr_info("trace data file %s created for %s\n",
		trace_file_name, reader->dev_name);

	if (pthread_create(&reader->thrd, NULL,
			   (void *)&reader_fn, &reader->param)) {
		pr_err("failed to create reader thread, %d\n", dev_id);
		return -4;
	}

	return 0;
}

static void destory_reader(reader_struct * reader)
{
	if (reader->thrd) {
		pthread_cancel(reader->thrd);
		if (pthread_join(reader->thrd, NULL) != 0)
			pr_err("failed to cancel thread[%lu]\n", reader->thrd);
		else
			reader->thrd = 0;
	}

	if (reader->param.sbuf) {
		munmap(reader->param.sbuf, MMAP_SIZE);
		reader->param.sbuf = NULL;
	}

	if (reader->dev_fd) {
		close(reader->dev_fd);
		reader->dev_fd = 0;
	}

	if (reader->param.trace_fd) {
		close(reader->param.trace_fd);
	}
}

static void handle_on_exit(void)
{
	uint32_t dev_id;

	/* if nothing to release */
	if (!(flags & FLAG_TO_REL))
		return;

	pr_info("exiting - to release resources...\n");

	foreach_dev(dev_id)
	    destory_reader(&reader[dev_id]);
}

static void signal_exit_handler(int sig)
{
	pr_info("exit on signal %d\n", sig);
	exit(0);
}

int main(int argc, char *argv[])
{
	uint32_t dev_id = 0;
	int err;

	/* parse options */
	if (parse_opt(argc, argv))
		exit(EXIT_FAILURE);

	dev_cnt = get_dev_cnt();
	if (dev_cnt == 0) {
		pr_err("Failed to find acrn trace devices, please check whether module acrn_trace is inserted\n");
		exit(EXIT_FAILURE);
	}

	reader = calloc(1, sizeof(reader_struct) * dev_cnt);
	if (!reader) {
		pr_err("Failed to allocate reader memory\n");
		exit(EXIT_FAILURE);
	}

	/* create dir for trace file */
	if (create_trace_file_dir(trace_file_dir)) {
		pr_err("Failed to create dir for trace files\n");
		exit(EXIT_FAILURE);
	}

	/* Set timer if timeout configured */
	if (timeout) {
		err = init_timer(timeout);
		if (err < 0) {
			pr_err("Failed to set timer\n");
			exit(EXIT_FAILURE);
		}
	}

	atexit(handle_on_exit);

	/* acquair res for each trace dev */
	flags |= FLAG_TO_REL;
	foreach_dev(dev_id)
	    if (create_reader(&reader[dev_id], dev_id) < 0)
		goto out_free;

	/* for kill exit handling */
	signal(SIGTERM, signal_exit_handler);
	signal(SIGINT, signal_exit_handler);

	/* wait for user input to stop */
	printf("q <enter> to quit:\n");
	while (!exiting && getchar() != 'q')
		printf("q <enter> to quit:\n");

 out_free:
	foreach_dev(dev_id)
	    destory_reader(&reader[dev_id]);

	free(reader);
	flags &= ~FLAG_TO_REL;

	return EXIT_SUCCESS;
}
