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

#define VM_WARNING_LINES 2000
#define LOOP_DEV_MAX 8

#define ANDROID_EVT_KEY_LEN 20

static const char *android_img = "/data/android/android.img";
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
	char vm_history[PATH_MAX];
	unsigned long size;
	int ret;
	int sid;

	if (!vm || !sender)
		return -1;

	sid = sender_id(sender);
	if (sid == -1)
		return -1;

	snprintf(vm_history, sizeof(vm_history), "/tmp/%s_%s",
		 "vm_hist", vm->name);

	if (get_file_size(vm_history) == (int)vm->history_size[sid])
		return 0;

	if (*data)
		free(*data);
	ret = read_full_binary_file(vm_history, &size, data);
	if (ret) {
		LOGE("read (%s) with error (%s)\n", vm_history,
		     strerror(errno));
		*data = NULL;
		return ret;
	}

	ret = strcnt(*data, '\n');
	if (ret > VM_WARNING_LINES) {
		LOGW("File too large, (%d) lines in (%s)\n",
		     ret, vm_history);
	}

	vm->history_size[sid] = size;

	return size;
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

		if (!vm->online)
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

			if (!vm->online)
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

		if (!vm->online)
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

static int is_data_partition(char *loop_dev)
{
	int ret = 0;
	char *out = exec_out2mem("debugfs -R \"ls\" %s", loop_dev);

	if (!out) {
		LOGE("debugfs -R ls %s failed, error (%s)\n", loop_dev,
		     strerror(errno));
		return 0;
	}

	if (strstr(out, "app-lib") &&
	    strstr(out, "tombstones") &&
	    strstr(out, "dalvik-cache")) {
		ret = 1;
	}

	free(out);
	return ret;
}

static char *setup_loop_dev(void)
{

	/* Currently UOS image(/data/android/android.img) mounted by
	 * launch_UOS.sh, we need mount its data partition to loop device
	 */
	char *out;
	char *end;
	char loop_dev_tmp[32];
	int i;

	if (!file_exists(android_img)) {
		LOGW("img(%s) is not available\n", android_img);
		return NULL;
	}

	loop_dev = exec_out2mem("losetup -f");
	if (!loop_dev) {
		/* get cmd out fail */
		LOGE("losetup -f failed, error (%s)\n",
		     strerror(errno));
		return NULL;
	} else if (strncmp(loop_dev, "/dev/loop", strlen("/dev/loop"))) {
		/* it's not a loop dev */
		LOGE("get error loop dev (%s)\n", loop_dev);
		goto free_loop_dev;
	} else if (strncmp(loop_dev, "/dev/loop0", strlen("/dev/loop0"))) {
		/* it's not loop0, was img mounted? */
		for (i = 0; i < LOOP_DEV_MAX; i++) {
			snprintf(loop_dev_tmp, ARRAY_SIZE(loop_dev_tmp),
				 "/dev/loop%d", i);
			if (is_data_partition(loop_dev_tmp)) {
				free(loop_dev);
				loop_dev = strdup(loop_dev_tmp);
				if (!loop_dev) {
					LOGE("out of memory\n");
					return NULL;
				}
				return loop_dev;
			}
		}
	}
	end = strchr(loop_dev, '\n');
	if (end)
		*end = 0;

	out = exec_out2mem("fdisk -lu %s", android_img);
	if (!out) {
		LOGE("fdisk -lu %s failed, error (%s)\n", android_img,
		     strerror(errno));
		goto free_loop_dev;
	}

	/* find data partition, sector unit = 512 bytes */
	char partition_start[32];
	char sectors[32];
	unsigned long pstart;
	char *partition;
	const char * const partition_fmt =
		IGN_ONEWORD "%31[^ ]" IGN_SPACES
		IGN_ONEWORD "%31[^ ]" IGN_RESTS;
	char *cursor = out;
	int ret;

	while (cursor &&
	       (partition = strstr(cursor, android_img))) {
		cursor = strchr(partition, '\n');
		ret = sscanf(partition, partition_fmt,
			     partition_start, sectors);
		if (ret != 2)
			continue;

		LOGD("start (%s) sectors(%s)\n", partition_start, sectors);
		/* size < 1G */
		if (atoi(sectors) < 1 * 2 * 1024 * 1024)
			continue;

		pstart = atol(partition_start) * 512;
		if (pstart == 0)
			continue;

		ret = exec_out2file(NULL, "losetup -o %lu %s %s",
				   pstart, loop_dev, android_img);
		/* if error occurs, is_data_partition will return false,
		 * only print error message here.
		 */
		if (ret != 0)
			LOGE("(losetup -o %lu %s %s) failed, return %d\n",
			     pstart, loop_dev, android_img, ret);

		if (is_data_partition(loop_dev)) {
			goto success;
		} else {
			ret = exec_out2file(NULL, "losetup -d %s", loop_dev);
			/* may lose a loop dev */
			if (ret != 0)
				LOGE("(losetup -d %s) failed, return %d\n",
				     loop_dev, ret);
			sleep(1);
		}
	}

	free(out);
free_loop_dev:
	free(loop_dev);

	return NULL;

success:
	free(out);
	return loop_dev;
}

static int ping_vm_fs(char *loop_dev)
{
	int id;
	int res;
	int count = 0;
	struct vm_t *vm;
	struct mm_file_t *vm_hist;
	char vm_history[PATH_MAX];
	char cmd[512];
	const char prefix[] = "#V1.0 CURRENTUPTIME";

	/* ensure history_event in uos available */
	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		snprintf(vm_history, sizeof(vm_history), "/tmp/%s_%s",
			 "vm_hist", vm->name);
		snprintf(cmd, sizeof(cmd),
			 "dump logs/history_event %s", vm_history);

		res = remove(vm_history);
		if (res == -1 && errno != ENOENT)
			LOGE("remove %s failed\n", vm_history);

		res = debugfs_cmd(loop_dev, cmd, NULL);
		if (res) {
			vm->online = 0;
			continue;
		}

		vm_hist = mmap_file(vm_history);
		if (vm_hist == NULL) {
			vm->online = 0;
			LOGE("%s(%s) unavailable\n", vm->name, vm_history);
			continue;
		}

		if (vm_hist->size &&
		    !strncmp(vm_hist->begin, prefix, strlen(prefix))) {
			vm->online = 1;
			count++;
		} else {
			vm->online = 0;
			LOGE("%s(%s) unavailable\n", vm->name, vm_history);
		}

		unmap_file(vm_hist);
	}
	return count;
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
	int ret;
	int id;
	struct vm_t *vm;
	static void *data[VM_MAX];

	if (!loop_dev) {
		loop_dev = setup_loop_dev();
		if (!loop_dev)
			return;
		LOGI("setup loop dev successful\n");
	}

	if (sender_id(sender) == 0) {
		ret = ping_vm_fs(loop_dev);
		if (!ret)
			return;
	}

	get_last_line_synced(sender);

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;

		if (!vm->online)
			continue;

		get_vm_history(vm, sender, &data[id]);
	}

	sync_lines_stage2(sender, (const void **)data, fn);
	sync_lines_stage1(sender, (const void **)data);
}
