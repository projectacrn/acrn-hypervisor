/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>
#include "load_conf.h"
#include "fsutils.h"
#include "crash_reclassify.h"
#include "sender.h"
#include "event_queue.h"
#include "event_handler.h"
#include "channels.h"
#include "log_sys.h"
#include "version.h"

#define CONFIG_INSTALL "/usr/share/defaults/telemetrics/acrnprobe.xml"
#define CONFIG_CUSTOMIZE "/etc/acrnprobe.xml"

void usage(void)
{
	printf("[Usage]\n");
	printf("\tacrnprobe -c [configuration file path] [-hV]\n");
	printf("[Options]\n");
	printf("\t-c,  --config         Configuration file\n");
	printf("\t-h,  --help           print the help message\n");
	printf("\t-V,  --version        Print the program version\n");
}

static void uptime(struct sender_t *sender)
{
	int fd;
	int frequency;
	struct uptime_t *uptime;

	uptime = sender->uptime;
	frequency = atoi(uptime->frequency);
	sleep(frequency);
	fd = open(uptime->path, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		LOGE("open uptime_file with (%d, %s) failed, error (%s)\n",
				atoi(uptime->frequency), uptime->path,
				strerror(errno));
	else
		close(fd);
}

int main(int argc, char *argv[])
{
	int ret;
	int id;
	int op;
	struct sender_t *sender;
	char cfg[PATH_MAX] = {0};
	char *config_path[2] = {CONFIG_CUSTOMIZE,
				CONFIG_INSTALL};
	struct option opts[] = {
		{ "config", required_argument, NULL, 'c' },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	while ((op = getopt_long(argc, argv, "c:hV", opts,
				 NULL)) != -1) {
		switch (op) {
		case 'c':
			strcpy(cfg, optarg);
			break;
		case 'h':
			usage();
			return 0;
		case 'V':
			printf("version is %d.%d-%s, build by %s@%s\n",
				AP_MAJOR_VERSION, AP_MINOR_VERSION,
				AP_BUILD_VERSION, AP_BUILD_USER,
				AP_BUILD_TIME);
			return 0;
		case '?':
			usage();
			return -1;
		}
	}

	if (!cfg[0]) {
		if (file_exists(config_path[0]))
			strcpy(cfg, config_path[0]);
		else
			strcpy(cfg, config_path[1]);
	}

	ret = load_conf(cfg);
	if (ret)
		return -1;

	init_crash_reclassify();
	ret = init_sender();
	if (ret)
		return -1;

	init_event_queue();
	ret = init_event_handler();
	if (ret)
		return -1;

	ret = init_channels();
	if (ret)
		return -1;

	while (1) {
		for_each_sender(id, sender, conf) {
			if (!sender)
				continue;
			uptime(sender);
		}
	}
	return 0;
}
