/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LOAD_CONF_H__
#define __LOAD_CONF_H__

#define SENDER_MAX 3

struct uptime_t {
	char *name;
	char *frequency;
	char *eventhours;

	int wd;
	char *path;
};

struct sender_t {
	struct uptime_t *uptime;
};

struct conf_t {
	struct sender_t *sender[SENDER_MAX];
};

struct conf_t conf;

#define for_each_sender(id, sender, conf) \
		for (id = 0, sender = conf.sender[0]; \
		     id < SENDER_MAX; \
		     id++, sender = conf.sender[id])

int load_conf(char *path);

#endif
