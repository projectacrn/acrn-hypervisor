/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <openssl/sha.h>
#include <time.h>
#include "property.h"
#include "fsutils.h"
#include "history.h"
#include "load_conf.h"
#include "log_sys.h"
#include "probeutils.h"

#define CRASH_CURRENT_LOG       "currentcrashlog"
#define STATS_CURRENT_LOG       "currentstatslog"
#define VM_CURRENT_LOG		"currentvmlog"

unsigned long long get_uptime(void)
{
	 static long long time_ns = -1;
	 struct timespec ts;

	 clock_gettime(CLOCK_BOOTTIME, &ts);
	 time_ns = (long long)ts.tv_sec * 1000000000LL +
		   (long long)ts.tv_nsec;

	return time_ns;
}

int get_uptime_string(char newuptime[24], int *hours)
{
	long long tm;
	int seconds, minutes;

	tm = get_uptime();

	/* seconds */
	*hours = (int)(tm / 1000000000LL);
	seconds = *hours % 60;

	/* minutes */
	*hours /= 60;
	minutes = *hours % 60;

	/* hours */
	*hours /= 60;

	return snprintf(newuptime, 24, "%04d:%02d:%02d", *hours,
			minutes, seconds);
}

int get_current_time_long(char buf[32])
{
	time_t t;
	struct tm *time_val;

	time(&t);
	time_val = localtime((const time_t *)&t);

	strftime(buf, 32, "%Y-%m-%d/%H:%M:%S  ", time_val);

	return 0;
}

/**
 * Compute a key with 20 characters
 *
 * @param[out] key The result key.
 * @param seed1 Seed.
 * @param seed2 Seed.
 *
 * @return 0 if successful, or -1 if not.
 */
static int compute_key(char *key, char *seed1, char *seed2)
{
	static SHA_CTX *sha;
	char buf[256] = {'\0',};
	long long time_ns = 0;
	char *tmp_key = key;
	unsigned char results[SHA_DIGEST_LENGTH];
	int i;
	int ret;

	if (sha == NULL) {
		sha = (SHA_CTX *)malloc(sizeof(SHA_CTX));
		if (sha == NULL) {
			LOGE("cannot create SHA_CTX memory...\n");
			return -1;
		}

		ret = SHA1_Init(sha);
		if (ret != 1) {
			LOGE("SHA1_Init failed, error (%s)\n",
			     strerror(errno));
			free(sha);
			sha = NULL;
			return -1;
		}
	}

	if (!key || !seed1 || !seed2)
		return -1;

	time_ns = get_uptime();
	snprintf(buf, 256, "%s%s%s%s%lld", gbuildversion, guuid, seed1,
		 seed2, time_ns);

	ret = SHA1_Update(sha, (unsigned char *)buf, strlen(buf));
	if (ret != 1) {
		LOGE("SHA1_Update failed, error (%s)\n",
		     strerror(errno));
		return -1;
	}

	ret = SHA1_Final(results, sha);
	if (ret != 1) {
		LOGE("SHA1_Final failed, error (%s)\n",
		     strerror(errno));
		return -1;
	}

	for (i = 0; i < SHA_DIGEST_LENGTH / 2; i++) {
		sprintf(tmp_key, "%02x", results[i]);
		tmp_key += 2;
	}
	*tmp_key = 0;

	return 0;
}

/**
 * Compute a key with 32 characters
 *
 * @param[out] key The result key.
 * @param seed Seed.
 *
 * @return 0 if successful, or -1 if not.
 */
static int compute_key256(char *key, char *seed)
{
	static SHA256_CTX *sha;
	char buf[256] = {'\0',};
	long long time_ns = 0;
	char *tmp_key = key;
	unsigned char results[SHA256_DIGEST_LENGTH];
	int i;
	int ret;

	if (sha == NULL) {
		sha = (SHA256_CTX *)malloc(sizeof(SHA256_CTX));
		if (sha == NULL) {
			LOGE("cannot create SHA256_CTX memory...\n");
			return -1;
		}

		ret = SHA256_Init(sha);
		if (ret != 1) {
			LOGE("SHA256_Init failed, error (%s)\n",
			     strerror(errno));
			free(sha);
			sha = NULL;
			return -1;
		}
	}

	if (!key || !seed)
		return -1;

	time_ns = get_uptime();
	snprintf(buf, 256, "%s%s%s%lld", gbuildversion, guuid, seed,
		 time_ns);

	ret = SHA256_Update(sha, (unsigned char *)buf, strlen(buf));
	if (ret != 1) {
		LOGE("SHA256_Update failed, error (%s)\n",
		     strerror(errno));
		return -1;
	}

	ret = SHA256_Final(results, sha);
	if (ret != 1) {
		LOGE("SHA256_Final failed, error (%s)\n",
		     strerror(errno));
		return -1;
	}

	for (i = 0; i < SHA256_DIGEST_LENGTH / 2; i++) {
		sprintf(tmp_key, "%02x", results[i]);
		tmp_key += 2;
	}
	*tmp_key = 0;

	return 0;
}

/**
 * Generate a event id with 20 characters
 *
 * @param seed1 Seed.
 * @param seed2 Seed.
 *
 * @return a pointer to result haskkey if successful, or NULL if not.
 */
char *generate_event_id(char *seed1, char *seed2)
{
	int ret;
	char *key = (char *)malloc(SHA_DIGEST_LENGTH + 1);

	if (!key)
		return NULL;

	ret = compute_key(key, seed1, seed2);
	if (ret < 0) {
		LOGE("compute_key error\n");
		free(key);
		key = NULL;
	}

	return key;
}

/**
 * Generate a event id with 32 characters
 *
 * @param seed Seed.
 *
 * @return a pointer to result haskkey if successful, or NULL if not.
 */
char *generate_eventid256(char *seed)
{
	int ret;
	char *key = (char *)malloc(SHA256_DIGEST_LENGTH + 1);

	if (!key)
		return NULL;

	ret = compute_key256(key, seed);
	if (ret < 0) {
		LOGE("compute_key256 error\n");
		free(key);
		key = NULL;
	}

	return key;
}

/**
 * Reserve a dir for log storage.
 *
 * @param mode Mode for log storage.
 * @param[out] dir Prefix of dir path reserved.
 * @param[out] index of dir reserved.
 *
 * @return 0 if successful, or -1 if not.
 */
static int reserve_log_folder(enum e_dir_mode mode, char *dir,
				unsigned int *current)
{
	char path[512];
	int res;
	struct sender_t *crashlog;
	char *outdir;
	unsigned int maxdirs;

	crashlog = get_sender_by_name("crashlog");
	outdir = crashlog->outdir;

	switch (mode) {
	case MODE_CRASH:
		sprintf(path, "%s/%s", outdir, CRASH_CURRENT_LOG);
		sprintf(dir, "%s/%s", outdir, "crashlog");
		break;
	case MODE_STATS:
		sprintf(path, "%s/%s", outdir, STATS_CURRENT_LOG);
		sprintf(dir, "%s/%s", outdir, "stats");
		break;
	case MODE_VMEVENT:
		sprintf(path, "%s/%s", outdir, VM_CURRENT_LOG);
		sprintf(dir, "%s/%s", outdir, "vmevent");
		break;
	default:
		LOGW("Invalid mode %d\n", mode);
		return -1;
	}

	/* Read current value in file */
	res = file_read_int(path, current);
	if (res < 0)
		return res;

	maxdirs = atoi(crashlog->maxcrashdirs);
	/* Open file in read/write mode to update the new current */
	res = file_update_int(path, *current, maxdirs);
	if (res < 0)
		return res;


	return 0;
}

#define strcat_fmt(buf, fmt, ...) \
(__extension__ \
({ \
	char __buf[1024] = {'\0',}; \
	snprintf(__buf, sizeof(__buf), fmt, ##__VA_ARGS__); \
	strcat(buf, __buf); \
}) \
)

/**
 * Create a crashfile with given params.
 *
 * @param dir Where to generate crashfile.
 * @param event Event name.
 * @param hashkey Event id.
 * @param type Subtype of this event.
 * @param data* String obtained by get_data.
 */
void generate_crashfile(char *dir, char *event, char *hashkey,
			char *type, char *data0,
			char *data1, char *data2)
{
	char *buf;
	char *path;
	char datetime[32];
	char uptime[32];
	int hours;
	int ret;
	const int fmtsize = 128;
	int filesize;

	get_current_time_long(datetime);
	get_uptime_string(uptime, &hours);

	filesize = fmtsize + strlen(event) +
		   strlen(hashkey) + strlen(guuid) +
		   strlen(datetime) + strlen(uptime) +
		   strlen(gbuildversion) + strlen(type);
	if (data0)
		filesize += strlen(data0);
	if (data1)
		filesize += strlen(data1);
	if (data2)
		filesize += strlen(data2);

	buf = malloc(filesize);
	if (buf == NULL) {
		LOGE("compute string failed, out of memory\n");
		return;
	}

	memset(buf, 0, filesize);
	strcat_fmt(buf, "EVENT=%s\n", event);
	strcat_fmt(buf, "ID=%s\n", hashkey);
	strcat_fmt(buf, "DEVICEID=%s\n", guuid);
	strcat_fmt(buf, "DATE=%s\n", datetime);
	strcat_fmt(buf, "UPTIME=%s\n", uptime);
	strcat_fmt(buf, "BUILD=%s\n", gbuildversion);
	strcat_fmt(buf, "TYPE=%s\n", type);
	if (data0)
		strcat_fmt(buf, "DATA0=%s\n", data0);
	if (data1)
		strcat_fmt(buf, "DATA1=%s\n", data1);
	if (data2)
		strcat_fmt(buf, "DATA2=%s\n", data2);
	strcat(buf, "_END\n");

	ret = asprintf(&path, "%s/%s", dir, "crashfile");
	if (ret < 0) {
		LOGE("compute string failed, out of memory\n");
		free(buf);
		return;
	}

	ret = overwrite_file(path, buf);
	if (ret)
		LOGE("new crashfile (%s) fail, error (%s)\n", path,
		     strerror(errno));

	free(buf);
	free(path);
}

/**
 * Create a dir for log storage.
 *
 * @param mode Mode for log storage.
 * @param hashkey Event id.
 *
 * @return a pointer to generated path if successful, or NULL if not.
 */
char *generate_log_dir(enum e_dir_mode mode, char *hashkey)
{
	char path[PATH_MAX];
	char dir[PATH_MAX];
	unsigned int current;
	int ret;

	ret = reserve_log_folder(mode, dir, &current);
	if (ret)
		return NULL;

	snprintf(path, sizeof(path), "%s%d_", dir, current);
	strncat(path, hashkey,
		(strlen(path) + strlen(hashkey) >= sizeof(path)) ?
		(sizeof(path) - strlen(path)) : strlen(hashkey));

	ret = mkdir(path, 0777);
	if (ret == -1) {
		LOGE("Cannot create dir %s\n", path);
		hist_raise_infoerror("DIR CREATE");
		return NULL;
	}

	return strdup(path);
}
