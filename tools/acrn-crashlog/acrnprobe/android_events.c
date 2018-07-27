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

/* Find the head of str, caller must guarantee that 'str' is not in
 * the first line.
 */
static char *line_head(const char *str)
{
	while (*str != '\n')
		str--;

	return (char *)(str + 1);
}

/* Find the next event that needs to be synced.
 * There is a history_event file in UOS side, it records UOS's events in
 * real-time. Generally, the cursor point to the first unsynchronized line.
 */
static char *vm_next_event_to_sync(const char *cursor, const struct vm_t *vm)
{
	char *line_to_sync = (char *)~(0);
	char *syncevent;
	char *p;
	char *type;
	char *subtype;
	char *target;
	int ret;
	int id;

	if (!cursor || !vm)
		return NULL;

	/* find all syncing types start from cursor,
	 * focus the event with smaller address.
	 */
	for_each_syncevent_vm(id, syncevent, vm) {
		if (!syncevent)
			continue;

		ret = asprintf(&type, "\n%s ", syncevent);
		if (ret < 0) {
			LOGE("calculate vm event failed, out of memory\n");
			return NULL;
		}
		/* a sync event may configured as type/subtype */
		p = strchr(type, '/');
		if (p) {
			*p = 0;
			subtype = p + 1;
		} else {
			subtype = NULL;
		}

		if (subtype)
			p = strstr(cursor, subtype);
		else
			p = strstr(cursor, type);

		if (p) {
			target = line_head(p);
			line_to_sync = MIN(line_to_sync, target);
		}

		free(type);
	}

	if (line_to_sync == (char *)~(0))
		return NULL;

	return line_to_sync;
}

#define VMRECORD_HEAD_LINES 6
#define VMRECORD_TAG_LEN 9
#define VMRECORD_TAG_WAITING_SYNC	"      <=="
#define VMRECORD_TAG_NOT_FOUND		"NOT_FOUND"
#define VMRECORD_TAG_SUCCESS		"         "
static int generate_log_vmrecord(const char *path)
{
	const char * const head =
		"/* DONT EDIT!\n"
		" * This file records VM id synced or about to be synched,\n"
		" * the tag \"<==\" indicates event waiting to sync.\n"
		" * the tag \"NOT_FOUND\" indicates event not found in UOS.\n"
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
					struct vm_t *vm,
					const char *key,
					enum stage1_refresh_type_t type)
{
	char log_new[64];
	char *log_vmrecordid;
	int sid;

	if (!key || !sender || !vm)
		return -1;

	sid = sender_id(sender);
	if (sid == -1)
		return -1;
	log_vmrecordid = sender->log_vmrecordid;
	/* the length of key must be 20, and its value can not be
	 * 00000000000000000000.
	 */
	if ((strlen(key) == ANDROID_EVT_KEY_LEN) &&
	    strcmp(key, "00000000000000000000")) {
		sprintf(vm->last_synced_line_key[sid],
			"%s", key);
		if (type == MM_ONLY)
			return 0;

		/* create a log file, so we can locate
		 * the right place in case of reboot
		 */
		if (!file_exists(log_vmrecordid))
			generate_log_vmrecord(log_vmrecordid);

		sprintf(log_new, "%s %s %s\n", vm->name, key,
			VMRECORD_TAG_WAITING_SYNC);
		append_file(log_vmrecordid, log_new);
		return 0;
	}

	LOGE("try to record an invalid key (%s) for (%s)\n",
	     key, vm->name);
	return -1;
}

enum stage2_refresh_type_t {
	SUCCESS,
	NOT_FOUND
};

static int refresh_key_synced_stage2(const struct mm_file_t *m_vm_records,
					const char *key,
					enum stage2_refresh_type_t type)
{
	char *lhead, *ltail;
	char *tag;

	if (!key || strlen(key) != ANDROID_EVT_KEY_LEN || !m_vm_records ||
	    m_vm_records->size <= 0)
		return -1;

	lhead = strstr(m_vm_records->begin, key);
	if (!lhead)
		return -1;

	ltail = strchr(lhead, '\n');
	if (!ltail)
		return -1;

	tag = strstr(lhead, VMRECORD_TAG_WAITING_SYNC);
	if (!tag || tag >= ltail)
		return -1;

	/* re-mark symbol "<==" for synced key */
	if (type == SUCCESS)
		memcpy(tag, VMRECORD_TAG_SUCCESS, VMRECORD_TAG_LEN);
	else if (type == NOT_FOUND)
		memcpy(tag, VMRECORD_TAG_NOT_FOUND, VMRECORD_TAG_LEN);
	else
		return -1;

	return 0;
}

static int get_vm_history(struct vm_t *vm, const struct sender_t *sender,
			void **data)
{
	unsigned long size;
	int ret;
	int sid;

	if (!vm || !sender || !data)
		return -1;

	sid = sender_id(sender);
	if (sid == -1)
		return -1;

	ret = e2fs_read_file_by_fpath(vm->datafs, android_histpath,
				      data, &size);
	if (ret == -1) {
		LOGE("failed to get vm_history from (%s).\n", vm->name);
		*data = NULL;
		return -1;
	}
	if (!size) {
		LOGE("empty vm_history from (%s).\n", vm->name);
		*data = NULL;
		return -1;
	}

	if (size == vm->history_size[sid])
		return 0;

	ret = strcnt(*data, '\n');
	if (ret > VM_WARNING_LINES)
		LOGW("File too large, (%d) lines in (%s) of (%s)\n",
		     ret, android_histpath, vm->name);

	vm->history_size[sid] = size;

	return 0;
}

static void sync_lines_stage1(const struct sender_t *sender, const void *data[])
{
	int id, sid;
	int ret;
	struct vm_t *vm;
	char *start;
	char *line_to_sync;
	char vmkey[ANDROID_WORD_LEN];
	const char * const vm_format =
		IGN_ONEWORD ANDROID_KEY_FMT IGN_RESTS;

	sid = sender_id(sender);
	if (sid == -1)
		return;

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		if (vm->last_synced_line_key[sid][0]) {
			start = strstr(data[id],
				       vm->last_synced_line_key[sid]);
			if (start == NULL) {
				LOGW("no synced id (%s), sync from head\n",
				     vm->last_synced_line_key[sid]);
				start = (char *)data[id];
			} else {
				start = next_line(start);
			}
		} else {
			start = (char *)data[id];
		}

		while ((line_to_sync = vm_next_event_to_sync(start, vm))) {
			/* It's possible that log's content isn't ready
			 * at this moment, so we postpone the fn until
			 * the next loop
			 */
			//fn(line_to_sync, vm);

			vmkey[0] = 0;
			ret = sscanf(line_to_sync, vm_format, vmkey);
			if (ret != 1) {
				LOGE("get an invalid line from (%s), skip\n",
				     vm->name);
				start = next_line(line_to_sync);
				continue;
			}

			LOGD("stage1 %s\n", vmkey);
			if (vmkey[0])
				refresh_key_synced_stage1(sender, vm,
							  vmkey, MM_FILE);
			start = next_line(line_to_sync);
		}
	}

}

static void sync_lines_stage2(const struct sender_t *sender, const void *data[],
			int (*fn)(const char*, const struct vm_t *))
{
	struct mm_file_t *m_vm_records;
	char *line;
	char *cursor;
	const char * const record_fmt =
		VM_NAME_FMT ANDROID_KEY_FMT IGN_RESTS;
	char vm_name[32];
	char vmkey[ANDROID_WORD_LEN];
	int id;
	struct vm_t *vm;
	int ret;

	m_vm_records = mmap_file(sender->log_vmrecordid);
	if (!m_vm_records) {
		LOGE("mmap %s failed, strerror(%s)\n", sender->log_vmrecordid,
						       strerror(errno));
		return;
	}
	if (!m_vm_records->size ||
	    mm_count_lines(m_vm_records) < VMRECORD_HEAD_LINES) {
		LOGE("(%s) invalid\n", sender->log_vmrecordid);
		goto out;
	}

	cursor = strstr(m_vm_records->begin, " " VMRECORD_TAG_WAITING_SYNC);
	if (!cursor)
		goto out;

	line = line_head(cursor);
	while (line) {
		char *vm_hist_line;

		vmkey[0] = 0;
		/* VMNAME xxxxxxxxxxxxxxxxxxxx <== */
		ret = sscanf(line, record_fmt, vm_name, vmkey);
		if (ret != 2) {
			LOGE("parse vm record failed\n");
			goto out;
		}

		for_each_vm(id, vm, conf) {
			if (!vm)
				continue;

			if (strcmp(vm->name, vm_name))
				continue;

			vm_hist_line = strstr(data[id], vmkey);
			if (!vm_hist_line) {
				LOGE("mark vmevent(%s) as unfound,", vmkey);
				LOGE("history_event in UOS was recreated?\n");
				refresh_key_synced_stage2(m_vm_records, vmkey,
							  NOT_FOUND);
				break;
			}

			ret = fn(line_head(vm_hist_line), vm);
			if (!ret)
				refresh_key_synced_stage2(m_vm_records, vmkey,
							  SUCCESS);
		}

		cursor = next_line(line);
		if (!cursor)
			break;

		line = strstr(cursor, VMRECORD_TAG_WAITING_SYNC);
		if (!line)
			break;

		line = line_head(line);
	}

out:
	unmap_file(m_vm_records);
}

/* This function only for initialization */
static void get_last_line_synced(const struct sender_t *sender)
{
	int id;
	int sid;
	int ret;
	struct vm_t *vm;
	char *p;
	char vmkey[ANDROID_WORD_LEN];
	char vm_name[32];

	if (!sender)
		return;

	sid = sender_id(sender);
	if (sid == -1)
		return;

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		/* generally only exec for each vm once */
		if (vm->last_synced_line_key[sid][0])
			continue;

		snprintf(vm_name, sizeof(vm_name), "%s ", vm->name);
		ret = file_read_key_value_r(sender->log_vmrecordid, vm_name,
					    sizeof(vmkey), vmkey);
		if (ret == -ENOENT) {
			LOGD("no (%s), will generate\n",
			     sender->log_vmrecordid);
			generate_log_vmrecord(sender->log_vmrecordid);
			continue;
		} else if (ret == -ENOMSG) {
			LOGD("no vm record id for (%s)\n", vm->name);
			continue;
		} else if (ret < 0) {
			LOGE("read key-value in (%s) for (%s), error (%s)\n",
			     sender->log_vmrecordid, vm->name,
			     strerror(errno));
			continue;
		}
		p = strchr(vmkey, ' ');
		if (p)
			*p = 0;

		ret = refresh_key_synced_stage1(sender, vm, vmkey, MM_ONLY);
		if (ret < 0) {
			LOGE("get a non-key vm event (%s) for (%s)\n",
			     vmkey, vm->name);
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
		snprintf(loop_dev_tmp, ARRAY_SIZE(loop_dev_tmp),
			 "/dev/loop%d", i);
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
 * Note that: fn should return 0 to indicate event has been handled,
 *	      or fn will be called in a time loop until it returns 0.
 */
void refresh_vm_history(const struct sender_t *sender,
		int (*fn)(const char*, const struct vm_t *))
{
	int res;
	int id;
	struct vm_t *vm;
	void *data[VM_MAX];

	if (!loop_dev) {
		loop_dev = setup_loop_dev();
		if (!loop_dev)
			return;
		LOGI("setup loop dev successful\n");
	}

	get_last_line_synced(sender);

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		data[id] = 0;
		res = e2fs_open(loop_dev, &vm->datafs);
		if (res == -1)
			continue;

		get_vm_history(vm, sender, (void *)&vm->history_data);
		data[id] = vm->history_data;
	}

	sync_lines_stage2(sender, (const void **)data, fn);
	sync_lines_stage1(sender, (const void **)data);
	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		e2fs_close(vm->datafs);

		if (data[id])
			free(data[id]);
	}

}
