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
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#include "acrntrace.h"

/* default minimal amount free space (in MB) left on the disk */
static uint64_t disk_reserved = 512;

#define TIMER_ID	(128)
static uint32_t timeout = 0;

static int exiting = 0;

/* for opt */
static uint64_t period = 10000;
static const char optString[] = "i:hcr:t:";
static const char dev_name[] = "/dev/acrn_trace";

static uint32_t flags;
static char trace_file_dir[TRACE_FILE_DIR_LEN];

static reader_struct *reader;
static int pcpu_num = 0;

static void display_usage(void)
{
	printf("acrntrace - tool to collect ACRN trace data\n"
	       "[Usage] acrntrace [-i] [period in msec] [-ch]\n\n"
	       "[Options]\n"
	       "\t-h: print this message\n"
	       "\t-r: minimal amount (in MB) of free space kept on the disk\n"
	       "\t    before acrntrace stops\n"
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
			ret = atoi(optarg);
			if (ret <= 0 || ret >=1000) {
				pr_err("'-i' require integer between [1-999]\n");
				return -EINVAL;
			}
			period = ret * 1000;
			pr_dbg("Period is %lu\n", period);
			break;
		case 'r':
			ret = atoi(optarg);
			if (ret <=0) {
				pr_err("'-r' require integer greater than 0\n");
				return -EINVAL;
			}
			disk_reserved = ret;
			pr_dbg("Keeping %dMB of space on the disk\n", ret);
			break;
		case 't':
			ret = atoi(optarg);
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

static int get_cpu_num(void)
{

	char cmd[128];
	char buf[16];
	int ret;

	snprintf(cmd, sizeof(cmd), "ls %s_* | wc -l", dev_name);

	ret = shell_cmd(cmd, buf, sizeof(buf));
	if (ret <= 0) {
		pr_err("Faile to get cpu number, use default 4\n");
		return PCPU_NUM;
	}

	ret = atoi(buf);
	if (ret <= 0) {
		pr_err("Wrong cpu number, use default 4\n");
		return PCPU_NUM;
	}

	return ret;
}

static int create_trace_file_dir(char *dir)
{
	int status;
	char cmd[CMD_MAX_LEN];
	char time_str[TIME_STR_LEN];
	time_t timep;
	struct tm *p;

	time(&timep);
	p = localtime(&timep);
	if (p)
		snprintf(time_str, TIME_STR_LEN, "%d%02d%02d-%02d%02d%02d",
			 (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
			 p->tm_hour, p->tm_min, p->tm_sec);
	else
		snprintf(time_str, TIME_STR_LEN, "00000000-000000");

	pr_info("start tracing at %s\n", time_str);

	snprintf(dir, TRACE_FILE_DIR_LEN, "%s%s", TRACE_FILE_ROOT, time_str);

	memset(cmd, 0, CMD_MAX_LEN);
	snprintf(cmd, CMD_MAX_LEN, "%s %s", "mkdir -p ", dir);

	status = system(cmd);
	if (-1 == status)
		return -1;	/* failed to execute sh */

	pr_dbg("dir %s creted\n", dir);

	return WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
}

/* function executed in each consumer thread */
static void reader_fn(param_t * param)
{
	int ret;
	uint32_t cpuid = param->cpuid;
	int fd = param->trace_fd;
	shared_buf_t *sbuf = param->sbuf;
	trace_ev_t e;
	struct statvfs stat;
	uint64_t freespace;

	pr_dbg("reader thread[%lu] created for FILE*[0x%p]\n",
	       pthread_self(), fp);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* Clear the old data in sbuf */
	if (flags & FLAG_CLEAR_BUF)
		sbuf_clear_buffered(sbuf);

	while (1) {
		do {
			/* Check that filesystem has enough space */
			if (fstatvfs(fd, &stat)) {
				printf("Fail to get vfs stat.\n");
				exiting = 1;
				exit(EXIT_FAILURE);
			}

			freespace = stat.f_frsize * (uint64_t)stat.f_bfree;
			freespace >>= 20; /* Convert to MB */

			if (freespace <= disk_reserved) {
				printf("Disk space limit reached (free space:"
					"%luMB, limit %luMB).\n",
					freespace, disk_reserved);
				exiting = 1;
				exit(EXIT_FAILURE);
			}

			ret = sbuf_write(fd, sbuf);
		} while (ret > 0);

		usleep(period);
	}
}

static int create_reader(reader_struct * reader, uint32_t cpu)
{
	char trace_file_name[TRACE_FILE_NAME_LEN];

	snprintf(reader->dev_name, DEV_PATH_LEN, "%s_%u", dev_name, cpu);
	reader->param.cpuid = cpu;

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
		pr_err("mmap failed for cpu%d, errno %d\n", cpu, errno);
		reader->param.sbuf = NULL;
		return -2;
	}

	pr_dbg("sbuf[%d]:\nmagic_num: %lx\nele_num: %u\n ele_size: %u\n",
	       cpu, reader->param.sbuf->magic, reader->param.sbuf->ele_num,
	       reader->param.sbuf->ele_size);

	snprintf(trace_file_name, TRACE_FILE_NAME_LEN, "%s/%d", trace_file_dir,
		 cpu);
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
		pr_err("failed to create reader thread, %d\n", cpu);
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
	uint32_t cpu;

	/* if nothing to release */
	if (!(flags & FLAG_TO_REL))
		return;

	pr_info("exiting - to release resources...\n");

	foreach_cpu(cpu)
	    destory_reader(&reader[cpu]);
}

static void signal_exit_handler(int sig)
{
	pr_info("exit on signal %d\n", sig);
	exit(0);
}

int main(int argc, char *argv[])
{
	uint32_t cpu = 0;
	int err;

	/* parse options */
	if (parse_opt(argc, argv))
		exit(EXIT_FAILURE);

	/* how many cpus */
	pcpu_num = get_cpu_num();
	reader = calloc(1, sizeof(reader_struct) * pcpu_num);
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
	foreach_cpu(cpu)
	    if (create_reader(&reader[cpu], cpu) < 0)
		goto out_free;

	/* for kill exit handling */
	signal(SIGTERM, signal_exit_handler);
	signal(SIGINT, signal_exit_handler);

	/* wait for user input to stop */
	printf("q <enter> to quit:\n");
	while (!exiting && getchar() != 'q')
		printf("q <enter> to quit:\n");

 out_free:
	foreach_cpu(cpu)
	    destory_reader(&reader[cpu]);

	free(reader);
	flags &= ~FLAG_TO_REL;

	return EXIT_SUCCESS;
}
