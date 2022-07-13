/*
 * Copyright (C)2021-2022 Intel Corporation.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "log.h"
#include "config.h"

struct life_mngr_config life_conf;

int check_dir(const char *path, int flags)
{
	struct stat st;

	if (stat(path, &st)) {
		if (flags) {
			if (mkdir(path, 0666)) {
				LOG_PRINTF("Failed to create folder (%s)\n", path);
				return -1;
			}
			return 0;
		} else {
			LOG_PRINTF("%s doesn't exist!\n", path);
			return -1;
		}
	}

	if (S_ISDIR(st.st_mode))
		return 0;

	fprintf(stderr, "%s exists, and is not a directory!\n", path);
	return -1;
}
bool load_config(char *conf_path)
{
	int pos;
	char buf[MAX_FILE_LINE_LEN];
	char *key_str, *value_str;
	FILE *fd;
	int ret;

	ret = check_dir(LIFE_MNGR_CONFIG_FOLDER, CHK_CREAT);
	if (ret) {
		LOG_PRINTF("%s %d\r\n", __func__, __LINE__);
		return false;
	}
	fd = fopen(conf_path, "r");
	if (fd == NULL) {
		LOG_PRINTF("Failed to open config file: %s", conf_path);
		return false;
	}
	while (fgets(buf, MAX_FILE_LINE_LEN, fd) != NULL) {
		pos = 0;
		while (buf[pos] == ' ')
			pos++;

		key_str = &buf[pos];
		if (key_str[0] == '#' || key_str[0] == '\n')
			continue;

		(void) strtok_r(key_str, "=", &value_str);
		if (strlen(value_str) == 0) {
			LOG_PRINTF("Config file: config item (%s) is invalid\n", buf);
			continue;
		}
		value_str[strlen(value_str) - 1] = '\0';
		LOG_PRINTF("Config file: key=%s, value=%s\n", key_str, value_str);
		if (strncmp(VM_TYPE, (const char *)key_str, sizeof(VM_TYPE)) == 0)
			memcpy(life_conf.vm_type, value_str, strlen(value_str));
		else if (strncmp(VM_NAME, (const char *)key_str, sizeof(VM_NAME)) == 0)
			memcpy(life_conf.vm_name, value_str, strlen(value_str));
		else if (strncmp(DEV_NAME, (const char *)key_str, sizeof(DEV_NAME)) == 0)
			memcpy(life_conf.dev_names, value_str, strlen(value_str));
		else if (strncmp(ALLOW_TRIGGER_S5, (const char *)key_str,
				sizeof(ALLOW_TRIGGER_S5)) == 0)
			memcpy(life_conf.allow_trigger_s5, value_str, strlen(value_str));
		else if (strncmp(ALLOW_TRIGGER_SYSREBOOT, (const char *)key_str,
				sizeof(ALLOW_TRIGGER_SYSREBOOT)) == 0)
			memcpy(life_conf.allow_trigger_sysreboot, value_str, strlen(value_str));
		else
			LOG_PRINTF("Invalid item in the configuration file, key=%s, value=%s\n",
					key_str, value_str);
	}
	if (strlen(life_conf.vm_name) == 0) {
		LOG_WRITE("Invalid VM name in configuration file\n");
		return false;
	}
	return true;
}
