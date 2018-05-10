/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "load_conf.h"
#include "fsutils.h"
#include "strutils.h"
#include "log_sys.h"
#include "crash_reclassify.h"

/**
 * Check if file contains content or not.
 * This function couldn't use for binary file.
 *
 * @param file Starting address of file cache.
 * @param content String to be searched.
 *
 * @return 1 if find the same string, or 0 if not.
 */
static int has_content(char *file, char *content)
{
	if (content && strstr(file, content))
		return 1;

	return 0;
}

/**
 * Check if file contains all configured contents or not.
 * This function couldn't use for binary file.
 *
 * @param crash Crash need checking.
 * @param file Starting address of file cache.
 *
 * @return 1 if all configured strings were found, or 0 if not.
 */
static int crash_has_all_contents(struct crash_t *crash,
				char *file)
{
	int id;
	int ret = 1;
	char *content;

	for_each_content_crash(id, content, crash) {
		if (!content)
			continue;

		if (!has_content(file, content)) {
			ret = 0;
			break;
		}
	}

	return ret;
}

/**
 * Might content is a 2-D array, write as mc[exp][cnt]
 * This function implements the following algorithm:
 *
 * r_mc[exp] = has_content(mc[exp][0]) || has_content(mc[exp][1]) || ...
 * result = r_mc[0] && r_mc[1] && ...
 *
 * This function couldn't use for binary file.
 *
 * @param crash Crash need checking.
 * @param file Starting address of file cache.
 *
 * @return 1 if result is true, or 0 if false.
 */
static int crash_has_mightcontents(struct crash_t *crash,
				char *file)
{
	int ret = 1;
	int ret_exp;
	int expid, cntid;
	char **exp;
	char *content;

	for_each_expression_crash(expid, exp, crash) {
		if (!exp || !exp_valid(exp))
			continue;

		ret_exp = 0;
		for_each_content_expression(cntid, content, exp) {
			if (!content)
				continue;

			if (has_content(file, content)) {
				ret_exp = 1;
				break;
			}
		}
		if (ret_exp == 0) {
			ret = 0;
			break;
		}
	}

	return ret;
}

/**
 * Judge the type of crash, according to configured content/mightcontent.
 * This function couldn't use for binary file.
 *
 * @param crash Crash need checking.
 * @param file Starting address of file cache.
 *
 * @return 1 if file matches these strings configured in crash, or 0 if not.
 */
static int crash_reclassify(struct crash_t *crash, char *file)
{
	return crash_has_all_contents(crash, file) &&
		crash_has_mightcontents(crash, file);
}

static int _get_data(char *file, struct crash_t *crash, char **data, int index)
{
	char *search_key;
	char *value;
	char *end;
	int size;
	int max_size = 255;

	if (!data)
		return 0;

	*data = NULL;

	search_key = crash->data[index];
	if (!search_key)
		return 0;

	value = strrstr(file, search_key);
	if (!value)
		return 0;

	end = strchr(value, '\n');
	if (!end)
		return 0;

	size = MIN(max_size, end - value);
	*data =  malloc(size + 1);
	if (*data == NULL)
		return -ENOMEM;

	strncpy(*data, value, size);
	*(*data + size) = 0;
	return size;
}

/**
 * Get segment from file, according to 'data' configuread in crash.
 * This function couldn't use for binary file.
 *
 * @param file Starting address of file cache.
 * @param crash Crash need checking.
 * @param[out] data0 Searched result, according to 'data0' configuread in crash.
 * @param[out] data1 Searched result, according to 'data1' configuread in crash.
 * @param[out] data2 Searched result, according to 'data2' configuread in crash.
 *
 * @return 0 if successful, or errno if not.
 */
static int get_data(char *file, struct crash_t *crash,
			char **data0, char **data1, char **data2)
{
	int res;

	/* to find strings which match conf words */
	res = _get_data(file, crash, data0, 0);
	if (res < 0)
		goto fail;

	res = _get_data(file, crash, data1, 1);
	if (res < 0)
		goto free_data0;

	res = _get_data(file, crash, data2, 2);
	if (res < 0)
		goto free_data1;

	return 0;
free_data1:
	if (data1 && *data1)
		free(*data1);
free_data0:
	if (data0 && *data0)
		free(*data0);
fail:
	return res;
}

/**
 * Judge the crash type. We only got a root crash from channel, sometimes,
 * we need to calculate a more specific type.
 * This function reclassify the crash type by searching trigger file's content.
 * This function couldn't use for binary file.
 *
 * @param rcrash Root crash obtained from channel.
 * @param trfile Path of trigger file.
 * @param[out] data0 Searched result, according to 'data0' configuread in crash.
 * @param[out] data1 Searched result, according to 'data1' configuread in crash.
 * @param[out] data2 Searched result, according to 'data2' configuread in crash.
 *
 * @return a pointer to the calculated crash structure if successful,
 *	   or NULL if not.
 */
static struct crash_t *crash_reclassify_by_content(struct crash_t *rcrash,
						char *trfile, char **data0,
						char **data1, char **data2)
{
	int depth;
	int level;
	int ret;
	struct crash_t *crash;
	struct crash_t *ret_crash = NULL;
	void *file;
	unsigned long size;
	int id;

	if (!rcrash)
		return NULL;

	if (trfile) {
		ret = read_full_binary_file(trfile, &size, &file);
		if (ret == -1) {
			LOGE("read %s failed, error (%s)\n",
			     trfile, strerror(errno));
			return NULL;
		}
	} else
		return rcrash;

	/* traverse every crash from leaf, return the first crash we find
	 * consider that we have few CRASH TYPE, so just using this simple
	 * implementation.
	 */
	depth = crash_depth(rcrash);
	for (level = depth; level >= 0; level--) {
		for_each_crash(id, crash, conf) {
			if (!crash ||
			    crash->trigger != rcrash->trigger ||
			    crash->channel != rcrash->channel ||
			    crash->level != level)
				continue;

			if (crash_reclassify(crash, file)) {
				ret = get_data(file, crash, data0, data1,
					       data2);
				if (ret < 0) {
					LOGE("get data error, error (%s)\n",
					     strerror(-ret));
					goto fail_data;
				} else {
					ret_crash = crash;
					goto fail_data;
				}
			}
		}
	}

fail_data:
	free(file);

	return ret_crash;
}

/**
 * Initailize crash reclassify, we only got a root crash from channel,
 * sometimes, we need to get a more specific type.
 */
void init_crash_reclassify(void)
{
	int id;
	struct crash_t *crash;

	for_each_crash(id, crash, conf) {
		if (!crash || !is_root_crash(crash))
			continue;

		crash->reclassify = crash_reclassify_by_content;
	}
}
