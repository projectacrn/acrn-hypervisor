/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <openssl/sha.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include "android_events.h"
#include "strutils.h"
#include "cmdutils.h"
#include "log_sys.h"
#include "fsutils.h"
#include "history.h"
#include "loop.h"

#define VM_WARNING_LINES 2000

#define ANDROID_DATA_PAR_NAME "data"
#define ANDROID_EVT_KEY_LEN 20

/* TODO: hardcoding the img path here means that only one Android Guest OS
 * is supoorted at this moment. To support multiple Android Guest OS, this
 * path should be moved to structure vm_t and configurable.
 */
static const char *android_img = "/data/android/android.img";
static const char *android_histpath = "logs/history_event";
char *loop_dev;

/* Find the next event that needs to be synced.
 * There is a history_event file in UOS side, it records UOS's events in
 * real-time. Generally, the cursor point to the first unsynchronized line.
 */
static char *next_vm_event(const char *cursor, const char *data,
			size_t dlen, const struct vm_t *vm)
{
	char *line_to_sync = (char *)~(0);
	const char *syncevent;
	int id;

	if (!cursor || !vm)
		return NULL;

	/* find all syncing types start from cursor,
	 * focus the event with smaller address.
	 */
	for_each_syncevent_vm(id, syncevent, vm) {
		char *p;
		char *new;
		char *type;
		int tlen;
		size_t len;

		if (!syncevent)
			continue;

		tlen = asprintf(&type, "\n%s ", syncevent);
		if (tlen == -1) {
			LOGE("out of memory\n");
			return NULL;
		}
		/* a sync event may be configured as type/subtype */
		p = strchr(type, '/');
		if (p) {
			char *subtype;
			int stlen;

			tlen = p - type;
			stlen = asprintf(&subtype, " %s", p + 1);
			if (stlen == -1) {
				free(type);
				LOGE("out of memory\n");
				return NULL;
			}
			new = get_line(subtype, (size_t)stlen, data, dlen,
				       cursor, &len);
			free(subtype);
			/*
			 * ignore the result if 'line' does not start with
			 * 'type'.
			 */
			if (!new || memcmp(new, type + 1, tlen - 1) ||
			    *(new + tlen - 1) != ' ') {
				free(type);
				continue;
			}
		} else {
			new = get_line(type, (size_t)tlen, data, dlen,
				       cursor, &len);
		}

		if (new)
			line_to_sync = MIN(line_to_sync, new);

		free(type);
	}

	if (line_to_sync == (char *)~(0))
		return NULL;

	return line_to_sync;
}

#define VMRECORD_HEAD_LINES 7
#define VMRECORD_TAG_LEN 9
#define VMRECORD_TAG_WAITING_SYNC	"      <=="
#define VMRECORD_TAG_NOT_FOUND		"NOT_FOUND"
#define VMRECORD_TAG_MISS_LOG		"MISS_LOGS"
#define VMRECORD_TAG_SUCCESS		"         "
static int generate_log_vmrecord(const char *path)
{
	const char * const head =
		"/* DONT EDIT!\n"
		" * This file records VM id synced or about to be synched,\n"
		" * the tag \"<==\" indicates event waiting to sync.\n"
		" * the tag \"NOT_FOUND\" indicates event not found in UOS.\n"
		" * the tag \"MISS_LOGS\" indicates event miss logs in UOS.\n"
		" */\n\n";

	LOGD("Generate (%s)\n", path);
	return overwrite_file(path, head);
}

enum stage1_refresh_type_t {
	MM_ONLY,
	MM_FILE
};

/**
 * There are 2 stages in vm events sync.
 * Stage1: record events to log_vmrecordid file.
 * Stage2: call sender's callback for each recorded events.
 *
 * The design reason is to give UOS some time to log to storage.
 */
static int refresh_key_synced_stage1(const struct sender_t *sender,
					struct vm_t *vm, const char *key,
					size_t klen,
					enum stage1_refresh_type_t type)
{
	char log_new[64];
	char *log_vmrecordid;
	int nlen;

	log_vmrecordid = sender->log_vmrecordid;
	/* the length of key must be 20, and its value can not be
	 * 00000000000000000000.
	 */
	if ((klen == ANDROID_EVT_KEY_LEN) &&
	    strcmp(key, "00000000000000000000")) {
		memcpy(vm->last_synced_line_key[sender->id], key, klen);
		vm->last_synced_line_key[sender->id][klen] = '\0';
		if (type == MM_ONLY)
			return 0;

		/* create a log file, so we can locate
		 * the right place in case of reboot
		 */
		if (!file_exists(log_vmrecordid))
			generate_log_vmrecord(log_vmrecordid);

		nlen = snprintf(log_new, sizeof(log_new), "%s %s %s\n",
				vm->name, key,
				VMRECORD_TAG_WAITING_SYNC);
		if (s_not_expect(nlen, sizeof(log_new))) {
			LOGE("failed to construct record, key (%s)\n", key);
			return -1;
		}

		if (append_file(log_vmrecordid, log_new,
				strnlen(log_new, 64)) < 0) {
			LOGE("failed to add new record (%s) to (%s)\n",
			     log_new, log_vmrecordid);
			return -1;
		}
		return 0;
	}

	LOGE("try to record an invalid key (%s) for (%s)\n",
	     key, vm->name);
	return -1;
}

enum stage2_refresh_type_t {
	SUCCESS,
	NOT_FOUND,
	MISS_LOG
};

static int refresh_key_synced_stage2(char *line, size_t len,
					enum stage2_refresh_type_t type)
{
	/* re-mark symbol "<==" for synced key */
	char *tag = line + len - VMRECORD_TAG_LEN;

	if (type == SUCCESS)
		memcpy(tag, VMRECORD_TAG_SUCCESS, VMRECORD_TAG_LEN);
	else if (type == NOT_FOUND)
		memcpy(tag, VMRECORD_TAG_NOT_FOUND, VMRECORD_TAG_LEN);
	else if (type == MISS_LOG)
		memcpy(tag, VMRECORD_TAG_MISS_LOG, VMRECORD_TAG_LEN);
	else
		return -1;

	return 0;
}

static int get_vms_history(const struct sender_t *sender)
{
	struct vm_t *vm;
	unsigned long size;
	int ret;
	int id;

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		if (e2fs_open(loop_dev, &vm->datafs) == -1)
			continue;

		if (e2fs_read_file_by_fpath(vm->datafs, android_histpath,
					    (void **)&vm->history_data,
					    &size) == -1) {
			LOGE("failed to get vm_history from (%s).\n", vm->name);
			vm->history_data = NULL;
			e2fs_close(vm->datafs);
			vm->datafs = NULL;
			continue;
		}
		if (!size) {
			LOGE("empty vm_history from (%s).\n", vm->name);
			vm->history_data = NULL;
			e2fs_close(vm->datafs);
			vm->datafs = NULL;
			continue;
		}

		/* warning large history file once */
		if (size == vm->history_size[sender->id])
			continue;

		ret = strcnt(vm->history_data, '\n');
		if (ret > VM_WARNING_LINES)
			LOGW("File too large, (%d) lines in (%s) of (%s)\n",
			     ret, android_histpath, vm->name);

		vm->history_size[sender->id] = size;
	}

	return 0;
}

static void sync_lines_stage1(const struct sender_t *sender)
{
	int id;
	struct vm_t *vm;

	for_each_vm(id, vm, conf) {
		char *data;
		size_t data_size;
		char *start;
		char *last_key;
		char *line_to_sync;

		if (!vm || !vm->history_data)
			continue;

		data = vm->history_data;
		data_size = vm->history_size[sender->id];
		last_key = &vm->last_synced_line_key[sender->id][0];
		if (*last_key) {
			start = strstr(data, last_key);
			if (start == NULL) {
				LOGW("no synced id (%s), sync from head\n",
				     last_key);
				start = data;
			} else {
				start = strchr(start, '\n');
			}
		} else {
			start = data;
		}

		while ((line_to_sync = next_vm_event(start, data, data_size,
						     vm))) {
			/* It's possible that log's content isn't ready
			 * at this moment, so we postpone the fn until
			 * the next loop
			 */
			//fn(line_to_sync, vm);
			char vmkey[ANDROID_WORD_LEN];
			ssize_t len;
			const char * const vm_format =
				IGN_ONEWORD ANDROID_KEY_FMT IGN_RESTS;

			len = strlinelen(line_to_sync,
					 data + data_size - line_to_sync);
			if (len == -1)
				break;

			if (str_split_ere(line_to_sync, len + 1, vm_format,
					  strlen(vm_format), vmkey,
					  sizeof(vmkey)) != 1) {
				LOGE("get an invalid line from (%s), skip\n",
				     vm->name);
				start = strchr(line_to_sync, '\n');
				continue;
			}

			LOGD("stage1 %s\n", vmkey);
			refresh_key_synced_stage1(sender, vm, vmkey,
						  strnlen(vmkey, sizeof(vmkey)),
						  MM_FILE);
			start = strchr(line_to_sync, '\n');
		}
	}

}

static char *next_record(const struct mm_file_t *file, const char *fstart,
			size_t *len)
{
	const char *tag = " " VMRECORD_TAG_WAITING_SYNC;
	size_t tlen = strlen(tag);

	return get_line(tag, tlen, file->begin, file->size, fstart, len);
}

static void sync_lines_stage2(const struct sender_t *sender,
			int (*fn)(const char*, size_t, const struct vm_t *))
{
	struct mm_file_t *recos;
	char *record;
	size_t recolen;

	recos = mmap_file(sender->log_vmrecordid);
	if (!recos) {
		LOGE("mmap %s failed, strerror(%s)\n", sender->log_vmrecordid,
						       strerror(errno));
		return;
	}
	if (!recos->size ||
	    mm_count_lines(recos) < VMRECORD_HEAD_LINES) {
		LOGE("(%s) invalid\n", sender->log_vmrecordid);
		goto out;
	}

	for (record = next_record(recos, recos->begin, &recolen); record;
	     record = next_record(recos, record + recolen, &recolen)) {
		const char * const record_fmt =
			VM_NAME_FMT ANDROID_KEY_FMT IGN_RESTS;
		char *hist_line;
		size_t len;
		char vm_name[32];
		char vmkey[ANDROID_WORD_LEN];
		struct vm_t *vm;
		int res;

		/* VMNAME xxxxxxxxxxxxxxxxxxxx <== */
		if (str_split_ere(record, recolen,
				  record_fmt, strlen(record_fmt),
				  vm_name, sizeof(vm_name),
				  vmkey, sizeof(vmkey)) != 2) {
			LOGE("failed to parse vm record\n");
			continue;
		}

		vm = get_vm_by_name((const char *)vm_name);
		if (!vm || !vm->history_data)
			continue;

		hist_line = get_line(vmkey, strnlen(vmkey, sizeof(vmkey)),
				     vm->history_data,
				     vm->history_size[sender->id],
				     vm->history_data, &len);
		if (!hist_line) {
			LOGW("mark vmevent(%s) as not-found\n", vmkey);
			refresh_key_synced_stage2(record, recolen, NOT_FOUND);
			continue;
		}

		res = fn(hist_line, len + 1, vm);
		if (res == VMEVT_HANDLED)
			refresh_key_synced_stage2(record, recolen, SUCCESS);
		else if (res == VMEVT_MISSLOG)
			refresh_key_synced_stage2(record, recolen, MISS_LOG);
	}

out:
	unmap_file(recos);
}

/* This function only for initialization */
static void get_last_line_synced(const struct sender_t *sender)
{
	int id;
	struct vm_t *vm;

	for_each_vm(id, vm, conf) {
		int ret;
		char *p;
		char vmkey[ANDROID_WORD_LEN];
		char vm_name[32];

		if (!vm)
			continue;

		/* generally only exec for each vm once */
		if (vm->last_synced_line_key[sender->id][0])
			continue;

		ret = snprintf(vm_name, sizeof(vm_name), "%s ", vm->name);
		if (s_not_expect(ret, sizeof(vm_name)))
			continue;

		ret = file_read_key_value_r(vmkey, sizeof(vmkey),
					    sender->log_vmrecordid,
					    vm_name, strnlen(vm_name, 32));
		if (ret == -ENOENT) {
			LOGD("(%s) does not exist, will generate it\n",
			     sender->log_vmrecordid);
			generate_log_vmrecord(sender->log_vmrecordid);
			continue;
		} else if (ret == -ENOMSG) {
			LOGD("couldn't find any records with (%s)\n", vm->name);
			continue;
		} else if (ret < 0) {
			LOGE("failed to search records in (%s), error (%s)\n",
			     sender->log_vmrecordid, strerror(errno));
			continue;
		}
		p = strchr(vmkey, ' ');
		if (p)
			*p = 0;

		ret = refresh_key_synced_stage1(sender, vm, vmkey,
						strnlen(vmkey, sizeof(vmkey)),
						MM_ONLY);
		if (ret < 0) {
			LOGE("invalid vm event (%s) in (%s)\n",
			     vmkey, sender->log_vmrecordid);
			continue;
		}
	}
}

static char *setup_loop_dev(void)
{

	/* Currently UOS image(/data/android/android.img) mounted by
	 * launch_UOS.sh, we need mount its data partition to loop device
	 */
	char loop_dev_tmp[32];
	int i;
	int res;
	int devnr;

	if (!file_exists(android_img)) {
		LOGW("img(%s) is not available\n", android_img);
		return NULL;
	}

	devnr = loopdev_num_get_free();
	for (i = 0; i < devnr; i++) {
		res = snprintf(loop_dev_tmp, ARRAY_SIZE(loop_dev_tmp),
			       "/dev/loop%d", i);
		if (s_not_expect(res, ARRAY_SIZE(loop_dev_tmp)))
			return NULL;

		if (loopdev_check_parname(loop_dev_tmp,
					  ANDROID_DATA_PAR_NAME)) {
			loop_dev = strdup(loop_dev_tmp);
			if (!loop_dev) {
				LOGE("out of memory\n");
				return NULL;
			}
			return loop_dev;
		}
	}

	res = asprintf(&loop_dev, "/dev/loop%d", devnr);
	if (res == -1) {
		LOGE("out of memory\n");
		return NULL;
	}

	res = loopdev_set_img_par(loop_dev, android_img, ANDROID_DATA_PAR_NAME);
	if (res == -1) {
		LOGE("failed to setup loopdev.\n");
		free(loop_dev);
		loop_dev = NULL;
		return NULL;
	}

	return loop_dev;
}

/* This function searches all android vms' new events and call the fn for
 * each event.
 *
 * Note that: fn should return VMEVT_HANDLED to indicate event has been handled.
 *	      fn will be called in a time loop if it returns VMEVT_DEFER.
 */
void refresh_vm_history(const struct sender_t *sender,
		int (*fn)(const char*, size_t, const struct vm_t *))
{
	struct vm_t *vm;
	int id;

	if (!sender)
		return;

	if (!loop_dev) {
		loop_dev = setup_loop_dev();
		if (!loop_dev)
			return;
		LOGI("setup loop dev successful\n");
	}

	get_last_line_synced(sender);
	get_vms_history(sender);

	sync_lines_stage2(sender, fn);
	sync_lines_stage1(sender);
	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;
		if (vm->history_data) {
			free(vm->history_data);
			vm->history_data = NULL;
		}
		if (vm->datafs) {
			e2fs_close(vm->datafs);
			vm->datafs = NULL;
		}
	}
}
