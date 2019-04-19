/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "log_sys.h"
#include "fsutils.h"
#include "strutils.h"
#include "vmrecord.h"
#include <stdlib.h>

int vmrecord_last(struct vmrecord_t *vmrecord, const char *vm_name,
		size_t nlen, char *vmkey, size_t ksize)
{
	int res;
	char *p;
	char *key;

	if (!vmrecord || !vm_name || !nlen || !vmkey || !ksize)
		return -1;

	key = malloc(nlen + 2);
	if (!key)
		return -1;

	memcpy(key, vm_name, nlen);
	key[nlen] = ' ';
	key[nlen + 1] = '\0';

	pthread_mutex_lock(&vmrecord->mtx);
	res = file_read_key_value_r(vmkey, ksize, vmrecord->path, key,
				    nlen + 1);
	pthread_mutex_unlock(&vmrecord->mtx);
	free(key);
	if (res < 0) {
		LOGE("failed to search %s, %s\n", vmrecord->path,
		     strerror(errno));
		return -1;
	}
	p = strchr(vmkey, ' ');
	if (p)
		*p = 0;

	return 0;
}

/* This function must be called with holding vmrecord mutex */
int vmrecord_mark(struct vmrecord_t *vmrecord, const char *vmkey,
			   size_t klen, enum vmrecord_mark_t type)
{
	size_t len;
	char *line;
	char *tag;

	if (!vmrecord || !vmrecord->recos || !vmkey || !klen)
		return -1;

	line = get_line(vmkey, klen, vmrecord->recos->begin,
			vmrecord->recos->size, vmrecord->recos->begin, &len);
	if (!line)
		return -1;

	tag = line + len - VMRECORD_TAG_LEN;

	if (type == SUCCESS)
		memcpy(tag, VMRECORD_TAG_SUCCESS, VMRECORD_TAG_LEN);
	else if (type == NOT_FOUND)
		memcpy(tag, VMRECORD_TAG_NOT_FOUND, VMRECORD_TAG_LEN);
	else if (type == MISS_LOG)
		memcpy(tag, VMRECORD_TAG_MISS_LOG, VMRECORD_TAG_LEN);
	else if (type == WAITING_SYNC)
		memcpy(tag, VMRECORD_TAG_WAITING_SYNC, VMRECORD_TAG_LEN);
	else if (type == ON_GOING)
		memcpy(tag, VMRECORD_TAG_ON_GOING, VMRECORD_TAG_LEN);
	else if (type == NO_RESRC)
		memcpy(tag, VMRECORD_TAG_NO_RESOURCE, VMRECORD_TAG_LEN);
	else
		return -1;

	return 0;

}

int vmrecord_open_mark(struct vmrecord_t *vmrecord, const char *vmkey,
			   size_t klen, enum vmrecord_mark_t type)
{
	int ret;

	if (!vmrecord || !vmkey || !klen)
		return -1;

	pthread_mutex_lock(&vmrecord->mtx);
	vmrecord->recos = mmap_file(vmrecord->path);
	if (!vmrecord->recos) {
		LOGE("failed to mmap %s, %s\n", vmrecord->path,
						strerror(errno));
		ret = -1;
		goto unlock;
	}
	if (!vmrecord->recos->size ||
	    mm_count_lines(vmrecord->recos) < VMRECORD_HEAD_LINES) {
		LOGE("(%s) invalid\n", vmrecord->path);
		ret = -1;
		goto out;
	}

	ret = vmrecord_mark(vmrecord, vmkey, klen, type);
out:
	unmap_file(vmrecord->recos);
unlock:
	pthread_mutex_unlock(&vmrecord->mtx);
	return ret;
}

int vmrecord_gen_ifnot_exists(struct vmrecord_t *vmrecord)
{
	const char * const head =
		"/* DONT EDIT!\n"
		" * This file records VM id synced or about to be synched,\n"
		" * the tag:\n"
		" * \"<==\" indicates event waiting to sync.\n"
		" * \"NOT_FOUND\" indicates event not found in UOS.\n"
		" * \"MISS_LOGS\" indicates event miss logs in UOS.\n"
		" * \"ON_GOING\" indicates event is under syncing.\n"
		" * \"NO_RESORC\" indicates no enough resources in SOS.\n"
		" */\n\n";

	if (!vmrecord) {
		LOGE("vmrecord was not initialized\n");
		return -1;
	}

	pthread_mutex_lock(&vmrecord->mtx);
	if (!file_exists(vmrecord->path)) {
		if (overwrite_file(vmrecord->path, head) < 0) {
			pthread_mutex_unlock(&vmrecord->mtx);
			LOGE("failed to create file (%s), %s\n",
			     vmrecord->path, strerror(errno));
			return -1;
		}
	}
	pthread_mutex_unlock(&vmrecord->mtx);
	return 0;
}

int vmrecord_new(struct vmrecord_t *vmrecord, const char *vm_name,
			  const char *key)
{
	char log_new[64];
	int nlen;

	if (!vmrecord || !vm_name || !key)
		return -1;

	nlen = snprintf(log_new, sizeof(log_new), "%s %s %s\n",
			vm_name, key, VMRECORD_TAG_WAITING_SYNC);
	if (s_not_expect(nlen, sizeof(log_new))) {
		LOGE("failed to construct record, key (%s)\n", key);
		return -1;
	}

	pthread_mutex_lock(&vmrecord->mtx);
	if (append_file(vmrecord->path, log_new, strnlen(log_new, 64)) < 0) {
		pthread_mutex_unlock(&vmrecord->mtx);
		LOGE("failed to append file (%s), %s\n", vmrecord->path,
		     strerror(errno));
		return -1;
	}
	pthread_mutex_unlock(&vmrecord->mtx);
	return 0;
}
