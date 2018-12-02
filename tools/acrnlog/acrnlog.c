/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#define LOG_ELEMENT_SIZE        80
#define LOG_MSG_SIZE		480
#define DEFAULT_POLL_INTERVAL	100000
#define LOG_INCOMPLETE_WARNING	"WARNING: logs missing here! "\
				"Try reducing polling interval"

/* Count of /dev/acrn_hvlog_cur_xxx */
static unsigned int dev_cnt;
static unsigned long interval = DEFAULT_POLL_INTERVAL;

/* this is for log file */
#define LOG_FILE_SIZE	(1024*1024)
#define LOG_FILE_NUM 	4
static size_t hvlog_log_size = LOG_FILE_SIZE;
static unsigned short hvlog_log_num = LOG_FILE_NUM;

struct hvlog_file {
	const char *path;
	int fd;

	size_t left_space;
	unsigned short index;
	unsigned short num;
};

static struct hvlog_file cur_log = {
	.path = "/tmp/acrnlog/acrnlog_cur",
	.fd = -1,
	.left_space = 0,
	.index = ~0,
	.num = LOG_FILE_NUM
};

static struct hvlog_file last_log = {
	.path = "/tmp/acrnlog/acrnlog_last",
	.fd = -1,
	.left_space = 0,
	.index = ~0,
	.num = LOG_FILE_NUM
};

struct hvlog_msg {
	__u64 usec;		/* timestamp, from tsc reset in usec */
	int cpu;		/* which physical cpu output the log */
	int sev;		/* log severity level */
	__u64 seq;		/* sequence num, used to reorder logs */

	size_t len;		/* length of message raw string */
	char raw[0];		/* raw log string, end with '\0' */
};

/*
 * describe the hvlog device, eg., /dev/acrn_hvlog_*
 * Note: this is thread-unsafe!
 */
struct hvlog_dev {
	int fd;
	struct hvlog_msg *msg;	/* pointer to msg */

	int latched;		/* 1 if an sbuf element latched */
	char entry_latch[LOG_ELEMENT_SIZE];	/* latch for an sbuf element */
	struct hvlog_msg latched_msg;	/* latch for parsed msg */
};

size_t write_log_file(struct hvlog_file * log, const char *buf, size_t len);

static int get_dev_cnt(void)
{
	char prefix[32] = "acrn_hvlog_cur_"; /* acrnlog dev prefix */
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
		ret = strstr(pdir->d_name, prefix);
		if (ret)
			cnt++;
	}

	closedir(dir);

	return cnt;
}

/*
 * The function read a complete msg from acrnlog dev.
 * read one more sbuf entry if read an entry doesn't end with '\0'
 * however, if the new entry contains a new msg - which means the ending char
 * is lost, it will be latched to be process next time.
 */
struct hvlog_msg *hvlog_read_dev(struct hvlog_dev *dev)
{
	char *p = NULL;
	int ret;
	size_t len = 0;
	struct hvlog_msg *msg[2];
	int msg_num;
	char warn_msg[LOG_MSG_SIZE] = {0};

	msg[0] = dev->msg;
	msg[1] = &dev->latched_msg;

	memset(msg[0], 0, sizeof(struct hvlog_msg) + LOG_MSG_SIZE);
	msg_num = 0;

	do {
		if (dev->latched) {
			/* handle the latched msg first */
			dev->latched = 0;
			memcpy(&msg[0]->raw[msg[0]->len], dev->entry_latch,
			       LOG_ELEMENT_SIZE);
			msg_num++;
			memcpy(msg[0], msg[1], sizeof(struct hvlog_msg));
		} else {
			ret =
			    read(dev->fd, &msg[0]->raw[msg[0]->len],
				 LOG_ELEMENT_SIZE);
			if (!ret)
				break;
			/* do we read a new meaasge?
			 * msg[0]->raw[msg[0]->len format: [%lluus][cpu=%d][sev=%d][seq=%llu]: */
			p = strstr(&msg[0]->raw[msg[0]->len], "][seq=");
			if (p) {
				p = p + strlen("][seq=");
				msg[msg_num]->seq = strtoull(p, NULL, 10);
				if ((errno == ERANGE && (msg[msg_num]->seq == ULLONG_MAX))
						|| (errno != 0 && msg[msg_num]->seq == 0)) {
					if (snprintf(warn_msg, LOG_MSG_SIZE, "\n\n\t%s[invalid seq]\n\n\n",
								LOG_INCOMPLETE_WARNING) >= LOG_MSG_SIZE) {
						printf("WARN: warning message is truncated\n");
					}
					write_log_file(&cur_log, warn_msg, strnlen(warn_msg, LOG_MSG_SIZE));
				} else {
					msg_num++;
					/* if we read another new msg, latch it */
					/* to process next time */
					if (msg_num > 1) {
						dev->latched = 1;
						memcpy(dev->entry_latch,
							   &msg[0]->raw[msg[0]->len],
							LOG_ELEMENT_SIZE);
						break;
					}
				}
			}
		}

		if (msg_num == 0) {
			/* if head of a message lost, continue to read */
			memset(msg[0], 0, sizeof(struct hvlog_msg)
			       + LOG_MSG_SIZE);
			continue;
		}

		len = strnlen(&msg[0]->raw[msg[0]->len], LOG_MSG_SIZE);
		msg[0]->len += len;
	} while (len == LOG_ELEMENT_SIZE &&
		 msg[0]->len < LOG_MSG_SIZE - LOG_ELEMENT_SIZE);

	if (!msg[0]->len)
		return NULL;

	msg[0]->raw[msg[0]->len] = '\n';
	msg[0]->raw[msg[0]->len + 1] = 0;
	msg[0]->len++;

	return msg[0];
}

struct hvlog_dev *hvlog_open_dev(const char *path)
{
	struct hvlog_dev *dev;

	dev = calloc(1, sizeof(struct hvlog_dev));
	if (!dev) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		goto open_dev;
	}

	dev->fd = open(path, O_RDONLY);
	if (dev->fd < 0) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		goto open_fd;
	}

	/* actual allocated size is 512B */
	dev->msg = calloc(1, sizeof(struct hvlog_msg) + LOG_MSG_SIZE);
	if (!dev->msg) {
		printf("%s %d\n", __FUNCTION__, __LINE__);
		goto alloc_msg;
	}

	return dev;

 open_fd:
	close(dev->fd);
 alloc_msg:
	free(dev);
 open_dev:
	return NULL;
}

void hvlog_close_dev(struct hvlog_dev *dev)
{
	if (!dev)
		return;

	if (dev->msg)
		free(dev->msg);
	if (dev->fd > 0)
		close(dev->fd);
	free(dev);
	dev = NULL;
}

/* this is for reading msg from hvlog devices array */
static struct hvlog_data {
	struct hvlog_dev *dev;
	struct hvlog_msg *msg;	/* clean it after use */
} *cur, *last;

/*
 * read the earliest msg from each dev, to hvlog_data[].msg
 * hvlog_data[] will be used for reordering
 */
static int hvlog_dev_read_msg(struct hvlog_data *data, int num_dev)
{
	int i, new_read;

	new_read = 0;
	for (i = 0; i < num_dev; i++) {
		if (data[i].msg)
			continue;
		if (!data[i].dev)
			continue;

		data[i].msg = hvlog_read_dev(data[i].dev);
		if (data[i].msg)
			new_read++;
	}

	return new_read;
}

static struct hvlog_msg *get_min_seq_msg(struct hvlog_data *data, int num_dev)
{
	int i, index_min = -1;
	__u64 min_seq = 0;
	struct hvlog_msg *msg;

	for (i = 0; i < num_dev; i++) {
		if (!data[i].msg)
			continue;

		if (index_min == -1) {
			index_min = i;
			min_seq = data[i].msg->seq;
			continue;
		}

		if (data[i].msg->seq > min_seq)
			continue;
		index_min = i;
		min_seq = data[i].msg->seq;
	}

	if (index_min == -1)
		return NULL;

	msg = data[index_min].msg;
	data[index_min].msg = NULL;

	return msg;
}

static int new_log_file(struct hvlog_file *log)
{
	char file_name[32] = { };

	if (log->fd >= 0) {
		if (!hvlog_log_size)
			return 0;
		close(log->fd);
		log->fd = -1;
	}

	if (snprintf(file_name, sizeof(file_name), "%s.%hu", log->path,
		 log->index + 1) >= sizeof(file_name)) {
		printf("WARN: log path is truncated\n");
	} else
		remove(file_name);

	log->fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (log->fd < 0) {
		perror(file_name);
		return -1;
	}

	log->left_space = hvlog_log_size;
	log->index++;
	if (snprintf(file_name, sizeof(file_name), "%s.%hu", log->path,
			log->index - hvlog_log_num) >= sizeof(file_name)) {
		printf("WARN: log path is truncated\n");
	} else
		remove(file_name);

	return 0;
}

size_t write_log_file(struct hvlog_file * log, const char *buf, size_t len)
{
	int ret;

	if (len >= log->left_space)
		if (new_log_file(log))
			return 0;

	ret = write(log->fd, buf, len);
	log->left_space -= ret;

	return ret;
}

static void *cur_read_func(void *arg)
{
	struct hvlog_msg *msg;
	__u64 last_seq = 0;
	char warn_msg[LOG_MSG_SIZE] = {0};

	while (1) {
		hvlog_dev_read_msg(cur, dev_cnt);
		msg = get_min_seq_msg(cur, dev_cnt);
		if (!msg) {
			usleep(interval);
			continue;
		}

		/* if msg->seq is not contineous, warn for logs missing */
		if (last_seq + 1 < msg->seq) {
			if (snprintf(warn_msg, LOG_MSG_SIZE,
				 "\n\n\t%s[%lu ms]\n\n\n",
				 LOG_INCOMPLETE_WARNING, interval) >= LOG_MSG_SIZE) {
				printf("WARN: warning message is truncated\n");
			}

			write_log_file(&cur_log, warn_msg, strnlen(warn_msg, LOG_MSG_SIZE));
		}

		last_seq = msg->seq;

		write_log_file(&cur_log, msg->raw, msg->len);
	}

	return NULL;
}

/* If dir *path does't exist, create a new one.
 * Otherwise, remove all the old acrnlog files in the dir.
 */
static int mk_dir(const char *path)
{
	char prefix[32] = "acrnlog_cur."; /* acrnlog file prefix */
	char acrnlog_file[64] = { };
	struct dirent *pdir;
	struct stat st;
	int index = 0;
	char *find;
	DIR *dir;

	if (stat(path, &st)) {
		if (mkdir(path, 0644))
			return -1;
	} else {
		/* Remove all the old acrnlogs */
		dir = opendir(path);
		if (!dir) {
			printf("Error opening directory %s. Error: %s\n",
				path, strerror(errno));
			return -1;
		}
		while ((pdir = readdir(dir)) != NULL) {
			find = strstr(pdir->d_name, prefix);
			if (!find)
				continue;

			if (snprintf(acrnlog_file, sizeof(acrnlog_file), "%s/%s%d",
					path, prefix, index++) >= sizeof(acrnlog_file)) {
				printf("WARN: acrnlog file path is truncated\n");
			} else
				remove(acrnlog_file);
		}
	}

	return 0;
}

/* for user optinal args */
static const char optString[] = "s:n:t:h";

static void display_usage(void)
{
	printf("acrnlog - tool to collect ACRN hypervisor log\n"
	       "[Usage] acrnlog [-s size] [-n number] [-t interval] [-h]\n\n"
	       "[Options]\n"
	       "\t-h: print this message\n"
	       "\t-t: polling interval to collect logs, in ms\n"
	       "\t-s: size limitation for each log file, in MB.\n"
	       "\t    0 means no limitation.\n"
	       "\t-n: how many files you would like to keep on disk\n"
	       "[Output] capatured log files under /tmp/acrnlog/\n");
}

static int parse_opt(int argc, char *argv[])
{
	int opt, ret;

	while ((opt = getopt(argc, argv, optString)) != -1) {
		switch (opt) {
		case 's':
			ret = strtoll(optarg, NULL, 10);
			if ((errno == ERANGE && (ret == LLONG_MAX || ret == LLONG_MIN))
					|| (errno != 0 && ret == 0)) {
				printf("'-s' invalid parameter: %s\n", optarg);
				return -EINVAL;
			}

			hvlog_log_size = ret * 1024;
			break;
		case 'n':
			ret = strtol(optarg, NULL, 10);
			if ((errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN))
					|| (errno != 0 && ret == 0)) {
				printf("'-n' invalid parameter: %s\n", optarg);
				return -EINVAL;
			}
			if (ret > 3)
				hvlog_log_num = ret;
			break;
		case 't':
			ret = strtol(optarg, NULL, 10);
			if (ret <= 0 || ret >=1000) {
				printf("'-t' require integer between [1-999]\n");
				return -EINVAL;
			}
			interval = ret * 1000;
			printf("Polling interval is %u ms\n", ret);
			break;
		case 'h':
			display_usage();
			return -EINVAL;
		default:
			/* Undefined operation. */
			display_usage();
			return -EINVAL;
		}
	}
	return 0;
}

static pthread_t cur_thread;

int main(int argc, char *argv[])
{
	char name[32];
	int i, ret;
	int num_cur, num_last;
	struct hvlog_msg *msg;

	if (parse_opt(argc, argv))
		return -1;

	ret = mk_dir("/tmp/acrnlog");
	if (ret) {
		printf("Cannot create /tmp/acrnlog. Error: %s\n",
			strerror(errno));
		return ret;
	}

	dev_cnt = get_dev_cnt();
	if (dev_cnt == 0) {
		printf("Failed to find acrn hvlog devices, please check whether module acrn_hvlog is inserted\n");
		return -1;
	}

	cur = calloc(dev_cnt, sizeof(struct hvlog_data));
	if (!cur) {
		printf("Failed to allocate buf for cur log buf\n");
		return -1;
	}

	last = calloc(dev_cnt, sizeof(struct hvlog_data));
	if (!last) {
		printf("Failed to allocate buf for last log buf\n");
		free(cur);
		return -1;
	}

	num_cur = 0;
	for (i = 0; i < dev_cnt; i++) {
		if (snprintf(name, sizeof(name), "/dev/acrn_hvlog_cur_%d", i) >= sizeof(name)) {
			printf("ERROR: cur hvlog path is truncated\n");
			return -1;
		}
		cur[i].dev = hvlog_open_dev(name);
		if (!cur[i].dev)
			perror(name);
		else
			num_cur++;
		cur[i].msg = NULL;
	}

	num_last = 0;
	for (i = 0; i < dev_cnt; i++) {
		if (snprintf(name, sizeof(name), "/dev/acrn_hvlog_last_%d", i) >= sizeof(name)) {
			printf("ERROR: last hvlog path is truncated\n");
			return -1;
		}
		last[i].dev = hvlog_open_dev(name);
		if (!last[i].dev)
			perror(name);
		else
			num_last++;
		last[i].msg = NULL;
	}

	printf("open cur:%d last:%d\n", num_cur, num_last);

	/* create thread to read cur log */
	if (num_cur) {
		ret = pthread_create(&cur_thread, NULL, cur_read_func, cur);
		if (ret) {
			printf("%s %d\n", __FUNCTION__, __LINE__);
			cur_thread = 0;
		}
	}

	if (num_last) {
		while (1) {
			hvlog_dev_read_msg(last, dev_cnt);
			msg = get_min_seq_msg(last, dev_cnt);
			if (!msg)
				break;
			write_log_file(&last_log, msg->raw, msg->len);
		}
	}

	if (cur_thread)
		pthread_join(cur_thread, NULL);

	for (i = 0; i < dev_cnt; i++) {
		hvlog_close_dev(cur[i].dev);
		hvlog_close_dev(last[i].dev);
	}

	free(cur);
	free(last);

	return 0;
}
