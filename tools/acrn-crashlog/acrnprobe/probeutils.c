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
#include "strutils.h"

#define CRASH_CURRENT_LOG       "currentcrashlog"
#define STATS_CURRENT_LOG       "currentstatslog"
#define VM_CURRENT_LOG		"currentvmlog"

#define BOOTID_NODE		"/proc/sys/kernel/random/boot_id"
#define BOOTID_LOG		"currentbootid"

unsigned long long get_uptime(void)
{
	long long time_ns;
	struct timespec ts;
	int res;

	res = clock_gettime(CLOCK_BOOTTIME, &ts);
	if (res == -1)
		return res;

	time_ns = (long long)ts.tv_sec * 1000000000LL +
		  (long long)ts.tv_nsec;

	return time_ns;
}

int get_uptime_string(char *newuptime, int *hours)
{
	long long tm;
	int seconds, minutes;
	int len;

	tm = get_uptime();
	if (tm == -1)
		return -1;

	/* seconds */
	*hours = (int)(tm / 1000000000LL);
	seconds = *hours % 60;

	/* minutes */
	*hours /= 60;
	minutes = *hours % 60;

	/* hours */
	*hours /= 60;

	len = snprintf(newuptime, UPTIME_SIZE, "%04d:%02d:%02d", *hours,
		       minutes, seconds);
	if (s_not_expect(len, UPTIME_SIZE))
		return -1;
	return 0;
}

int get_current_time_long(char *buf)
{
	time_t t;
	struct tm *time_val;

	time(&t);
	time_val = localtime((const time_t *)&t);
	if (!time_val)
		return -1;

	return strftime(buf, LONG_TIME_SIZE, "%Y-%m-%d/%H:%M:%S  ", time_val);
}

static int compute_key(char *key, size_t klen, const char *seed,
			const size_t slen)
{
	SHA256_CTX sha;
	char buf[VERSION_SIZE];
	int len;
	long long time_ns;
	char *tmp_key = key;
	unsigned char results[SHA256_DIGEST_LENGTH];
	size_t i;

	if (!key || !seed || !slen)
		return -1;
	if (klen > SHA256_DIGEST_LENGTH * 2 || !klen)
		return -1;

	SHA256_Init(&sha);
	time_ns = get_uptime();
	len = snprintf(buf, VERSION_SIZE, "%s%s%lld",
			gbuildversion, guuid, time_ns);
	if (s_not_expect(len , VERSION_SIZE))
		return -1;

	SHA256_Update(&sha, (unsigned char *)buf, strnlen(buf, VERSION_SIZE));
	SHA256_Update(&sha, (unsigned char *)seed, strnlen(seed, slen));

	SHA256_Final(results, &sha);

	for (i = 0; i < klen / 2; i++) {
		len = snprintf(tmp_key, 3, "%02x", results[i]);
		if (s_not_expect(len, 3))
			return -1;
		tmp_key += 2;
	}
	*tmp_key = 0;

	return 0;
}

/**
 * Generate an event id with specified type.
 *
 * @param seed1 Seed1.
 * @param seed2 Seed2, this parameter will be ignored if the value is NULL.
 * @param type The type of key. The length of generated id will be 20
 *		characters if type is KEY_SHORT; 32 characters if type is
 *		KEY_LONG.
 *
 * @return a pointer to result haskkey if successful, or NULL if not.
 */
char *generate_event_id(const char *seed1, size_t slen1, const char *seed2,
			size_t slen2, enum key_type type)
{
	int ret;
	char *buf;
	char *key;
	size_t klen;

	if (!seed1 || !slen1)
		return NULL;

	if (type == KEY_SHORT)
		klen = SHORT_KEY_LENGTH;
	else if (type == KEY_LONG)
		klen = LONG_KEY_LENGTH;
	else
		return NULL;

	key = (char *)malloc(klen + 1);
	if (!key) {
		LOGE("failed to generate event id, out of memory\n");
		return NULL;
	}

	if (seed2) {
		if (asprintf(&buf, "%s%s", seed1, seed2) == -1) {
			LOGE("failed to generate event id, out of memory\n");
			free(key);
			return NULL;
		}
		ret = compute_key(key, klen, (const char *)buf, slen1 + slen2);
		free(buf);
	} else {
		ret = compute_key(key, klen, seed1, slen1);
	}

	if (ret < 0) {
		LOGE("compute_key error\n");
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
	char path[PATH_MAX];
	int res;
	int plen;
	int dlen;
	struct sender_t *crashlog;
	const char *outdir;
	int maxdirs;

	crashlog = get_sender_by_name("crashlog");
	if (!crashlog)
		return -1;

	outdir = crashlog->outdir;

	switch (mode) {
	case MODE_CRASH:
		plen = snprintf(path, PATH_MAX, "%s/%s", outdir,
				CRASH_CURRENT_LOG);
		dlen = snprintf(dir, PATH_MAX, "%s/%s", outdir, "crashlog");
		break;
	case MODE_STATS:
		plen = snprintf(path, PATH_MAX, "%s/%s", outdir,
			STATS_CURRENT_LOG);
		dlen = snprintf(dir, PATH_MAX, "%s/%s", outdir, "stats");
		break;
	case MODE_VMEVENT:
		plen = snprintf(path, PATH_MAX, "%s/%s", outdir,
				VM_CURRENT_LOG);
		dlen = snprintf(dir, PATH_MAX, "%s/%s", outdir, "vmevent");
		break;
	default:
		LOGW("Invalid mode %d\n", mode);
		return -1;
	}

	if (s_not_expect(plen, PATH_MAX) || s_not_expect(dlen, PATH_MAX)) {
		LOGE("the length of path/dir is too long\n");
		return -1;
	}
	/* Read current value in file */
	res = file_read_int(path, current);
	if (res < 0)
		return res;

	if (cfg_atoi(crashlog->maxcrashdirs, crashlog->maxcrashdirs_len,
		     &maxdirs) == -1)
		return -1;
	if (maxdirs <= 0) {
		LOGE("failed to reserve dir, maxdirs must be greater than 0\n");
		return -1;
	}
	/* Open file in read/write mode to update the new current */
	res = file_update_int(path, *current, (unsigned int)maxdirs);
	if (res < 0)
		return res;


	return 0;
}

static char *cf_line(char *dest, const char *k, size_t klen, const char *v,
			size_t vlen)
{
	char *t;

	t = mempcpy(dest, k, klen);
	t = mempcpy(t, v, vlen);
	return mempcpy(t, "\n", 1);
}

/**
 * Create a crashfile with given params.
 *
 * @param dir Where to generate crashfile.
 * @param event Event name.
 * @param hashkey Event id.
 * @param type Subtype of this event.
 * @param data* String obtained by get_data.
 */
void generate_crashfile(const char *dir,
			const char *event, size_t elen,
			const char *hashkey, size_t hlen,
			const char *type, size_t tlen,
			const char *data0, size_t d0len,
			const char *data1, size_t d1len,
			const char *data2, size_t d2len)
{
	char *buf;
	char *path;
	char *tail;
	char datetime[LONG_TIME_SIZE];
	char uptime[UPTIME_SIZE];
	int hours;
	const int fmtsize = 128;
	size_t ltlen;
	int n;
	int filesize;

	if (!dir || !event || !elen || !hashkey || !hlen ||
	    !type || !tlen)
		return;
	if (d0len > 0 && !data0)
		return;
	if (d1len > 0 && !data1)
		return;
	if (d2len > 0 && !data2)
		return;

	ltlen = get_current_time_long(datetime);
	if (!ltlen)
		return;
	n = get_uptime_string(uptime, &hours);
	if (n < 0)
		return;

	filesize = fmtsize + ltlen + n + elen + hlen + tlen + d0len + d1len +
		   d2len + strnlen(guuid, UUID_SIZE) +
		   strnlen(gbuildversion, BUILD_VERSION_SIZE);

	buf = malloc(filesize);
	if (buf == NULL) {
		LOGE("out of memory\n");
		return;
	}

	tail = cf_line(buf, "EVENT=", 6, event, elen);
	tail = cf_line(tail, "ID=", 3, hashkey, hlen);
	tail = cf_line(tail, "DEVICEID=", 9, guuid, strnlen(guuid, UUID_SIZE));
	tail = cf_line(tail, "DATE=", 5, datetime, ltlen);
	tail = cf_line(tail, "UPTIME=", 7, uptime, n);
	tail = cf_line(tail, "BUILD=", 6, gbuildversion,
		       strnlen(gbuildversion, BUILD_VERSION_SIZE));
	tail = cf_line(tail, "TYPE=", 5, type, tlen);

	if (d0len)
		tail = cf_line(tail, "DATA0=", 6, data0, d0len);
	if (d1len)
		tail = cf_line(tail, "DATA1=", 6, data1, d1len);
	if (d2len)
		tail = cf_line(tail, "DATA2=", 6, data2, d2len);
	tail = mempcpy(tail, "_END\n", 5);
	*tail = '\0';

	if (asprintf(&path, "%s/crashfile", dir) == -1) {
		LOGE("out of memory\n");
		free(buf);
		return;
	}

	if (overwrite_file(path, buf) != 0)
		LOGE("failed to new crashfile (%s), error (%s)\n", path,
		     strerror(errno));

	free(buf);
	free(path);
}

/**
 * Create a dir for log storage.
 *
 * @param mode Mode for log storage.
 * @param hashkey Event id.
 * @param[out] dir_len Length of generated dir.
 *
 * @return a pointer to generated path if successful, or NULL if not.
 */
char *generate_log_dir(enum e_dir_mode mode, char *hashkey, size_t *dir_len)
{
	char *path;
	char dir[PATH_MAX];
	unsigned int current;
	int len;

	if (reserve_log_folder(mode, dir, &current))
		return NULL;

	len = asprintf(&path, "%s%d_%s", dir, current, hashkey);
	if (len == -1) {
		LOGE("construct log path failed, out of memory\n");
		hist_raise_infoerror("DIR CREATE", 10);
		return NULL;
	}

	if (mkdir(path, 0777) == -1) {
		LOGE("Cannot create dir %s\n", path);
		hist_raise_infoerror("DIR CREATE", 10);
		free(path);
		return NULL;
	}

	if (dir_len)
		*dir_len = (size_t)len;
	return path;
}

int is_boot_id_changed(void)
{
	void *boot_id;
	void *logged_boot_id;
	char logged_boot_id_path[PATH_MAX];
	unsigned long size;
	struct sender_t *crashlog;
	int res;
	int result = 1; /* returns changed by default */

	crashlog = get_sender_by_name("crashlog");
	if (!crashlog)
		return result;

	res = read_file(BOOTID_NODE, &size, &boot_id);
	if (res == -1 || !size)
		return result;

	res = snprintf(logged_boot_id_path, sizeof(logged_boot_id_path),
		       "%s/%s", crashlog->outdir, BOOTID_LOG);
	if (s_not_expect(res, sizeof(logged_boot_id_path)))
		goto out;

	if (file_exists(logged_boot_id_path)) {
		res = read_file(logged_boot_id_path, &size, &logged_boot_id);
		if (res == -1 || !size)
			goto out;

		if (!strcmp((char *)logged_boot_id, (char *)boot_id))
			result = 0;

		free(logged_boot_id);
	}

	if (result)
		overwrite_file(logged_boot_id_path, boot_id);
out:
	free(boot_id);
	return result;
}
