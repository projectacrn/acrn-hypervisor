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
static int has_content(const char *file, const char *content)
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
static int crash_has_all_contents(const struct crash_t *crash,
				const char *file)
{
	int id;
	int ret = 1;
	const char *content;

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
static int crash_has_mightcontents(const struct crash_t *crash,
				const char *file)
{
	int ret = 1;
	int ret_exp;
	int expid, cntid;
	const char * const *exp;
	const char *content;

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
static int crash_match_content(const struct crash_t *crash, const char *file)
{
	return crash_has_all_contents(crash, file) &&
		crash_has_mightcontents(crash, file);
}

static int _get_data(const char *file, const struct crash_t *crash,
			char **data, size_t *dsize, const int index)
{
	const char *search_key;
	char *value;
	char *end;
	char *data_new;
	ssize_t size;
	const size_t max_size = 255;

	search_key = crash->data[index];
	if (!search_key)
		goto empty;

	value = strrstr(file, search_key);
	if (!value)
		goto empty;

	end = strchr(value, '\n');
	if (!end)
		goto empty;

	size = MIN(max_size, (size_t)(end - value));
	if (!size)
		goto empty;

	data_new = realloc(*data, *dsize + size + 1);
	if (!data_new) {
		LOGE("failed to realloc\n");
		return -1;
	}

	strncpy(data_new + *dsize, value, size);
	*(data_new + *dsize + size) = 0;

	*data = data_new;
	*dsize += size;
	return 0;
empty:
	data_new = realloc(*data, *dsize + 1);
	if (!data_new) {
		LOGE("failed to realloc\n");
		return -1;
	}

	*(data_new + *dsize) = 0;

	*data = data_new;
	*dsize += 1;
	return 0;
}

/**
 * Get segment from file, according to 'data' configuread in crash.
 * This function couldn't use for binary file.
 *
 * @param file Starting address of file cache.
 * @param crash Crash need checking.
 * @param[out] data Searched result, according to 'data' configuread in crash.
 *
 * @return 0 if successful, or -1 if not.
 */
static int get_data(const char *file, const struct crash_t *crash,
			char **r_data, size_t *r_dsize)
{
	char *data = NULL;
	size_t dsize = 0;
	int i;

	/* to find strings which match conf words */
	for (i = 0; i < DATA_MAX; i++) {
		if (_get_data(file, crash, &data, &dsize, i) == -1)
			goto fail;
	}

	*r_data = data;
	*r_dsize = dsize;
	return 0;
fail:
	if (data)
		free(data);
	return -1;
}

static struct crash_t *crash_find_matched_child(const struct crash_t *crash,
						const char *rtrfmt)
{
	struct crash_t *child;
	struct crash_t *matched_child = NULL;
	int i;
	int count;
	int res;
	const char *trfile_fmt;
	char **trfiles;
	void *content;
	unsigned long size;

	if (!crash)
		return NULL;

	for_crash_children(child, crash) {
		if (!child->trigger)
			continue;

		if (!strcmp(child->trigger->type, "dir"))
			trfile_fmt = rtrfmt;
		else
			trfile_fmt = child->trigger->path;

		count = config_fmt_to_files(trfile_fmt, &trfiles);
		if (count <= 0)
			continue;

		for (i = 0; i < count; i++) {
			res = read_file(trfiles[i], &size, &content);
			if (res == -1) {
				LOGE("read %s failed, error (%s)\n",
				     trfiles[i], strerror(errno));
				continue;
			}
			if (!size)
				continue;
			if (crash_match_content(child, content)) {
				free(content);
				matched_child = child;
				break;
			}
			free(content);
		}

		for (i = 0; i < count; i++)
			free(trfiles[i]);
		free(trfiles);

		if (matched_child)
			break;
	}

	/* It returns the first matched crash */
	return matched_child;
}

/**
 * Judge the crash type. We only got a root crash from channel, sometimes,
 * we need to calculate a more specific type.
 * This function reclassify the crash type by searching trigger file's content.
 * This function couldn't use for binary file.
 *
 * @param rcrash Root crash obtained from channel.
 * @param rtrfile_fmt Path fmt of trigger file of root crash.
 * @param[out] data Searched result, according to 'data' configuread in crash.
 *
 * @return a pointer to the calculated crash structure if successful,
 *	   or NULL if not.
 */
static struct crash_t *crash_reclassify_by_content(const struct crash_t *rcrash,
					const char *rtrfile_fmt,
					char **data, size_t *dsize)
{
	int count;
	const struct crash_t *crash;
	const struct crash_t *ret_crash = rcrash;
	const char *trfile_fmt;
	char **trfiles;
	void *content;
	unsigned long size;
	int i;

	if (!rcrash || !data || !dsize)
		return NULL;

	crash = rcrash;

	while (1) {
		crash = crash_find_matched_child(crash, rtrfile_fmt);
		if (!crash)
			break;

		ret_crash = crash;
	}

	if (!strcmp(ret_crash->trigger->type, "dir"))
		trfile_fmt = rtrfile_fmt;
	else
		trfile_fmt = ret_crash->trigger->path;

	/* trfile may not be specified */
	if (!trfile_fmt)
		return (struct crash_t *)ret_crash;

	count = config_fmt_to_files(trfile_fmt, &trfiles);
	if (count <= 0)
		return (struct crash_t *)ret_crash;

	/* get data from last file */
	if (read_file(trfiles[count - 1], &size, &content) == -1) {
		LOGE("failed to read %s, error (%s)\n",
		     trfiles[count - 1], strerror(errno));
		goto free_files;
	}
	if (!size)
		goto free_files;

	if (get_data(content, ret_crash, data, dsize) == -1)
		LOGE("failed to get data\n");

	free(content);

free_files:
	for (i = 0; i < count; i++)
		free(trfiles[i]);
	free(trfiles);

	return (struct crash_t *)ret_crash;
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
		if (!crash)
			continue;

		crash->reclassify = crash_reclassify_by_content;
	}
}
