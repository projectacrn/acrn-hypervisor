/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <limits.h>
#include "fsutils.h"
#include "strutils.h"
#include "cmdutils.h"
#include "load_conf.h"
#include "sender.h"
#include "probeutils.h"
#include "android_events.h"
#include "history.h"
#include "property.h"
#include "startupreason.h"
#include "log_sys.h"
#include "loop.h"

#ifdef HAVE_TELEMETRICS_CLIENT
#include "telemetry.h"

#define CRASH_SEVERITY 4
#define INFO_SEVERITY 2

struct telemd_data_t {
	char *class;
	char *srcdir;
	char *eventid;
	uint32_t severity;
};
#endif

static int crashlog_check_space(void)
{
	struct sender_t *crashlog = get_sender_by_name("crashlog");
	int quota;
	size_t dsize;
	int cfg_size;


	if (!crashlog)
		return -1;

	if (cfg_atoi(crashlog->spacequota, crashlog->spacequota_len,
		     &quota) == -1)
		return -1;

	if (!space_available(crashlog->outdir, quota))
		return -1;

	if (dir_blocks_size(crashlog->outdir, crashlog->outdir_len,
			    &dsize) == -1) {
		LOGE("failed to check outdir size\n");
		return -1;
	}

	if (cfg_atoi(crashlog->foldersize, crashlog->foldersize_len,
		     &cfg_size) == -1)
		return -1;

	if (dsize/MB >= (size_t)cfg_size)
		return -1;

	return 0;
}

static int cal_log_filepath(char **out, const struct log_t *log,
				const char *srcname, const char *desdir)
{
	const char *filename;
	int need_timestamp = 0;
	int hours;
	char timebuf[UPTIME_SIZE];

	if (!out || !log || !desdir)
		return -1;

	if (is_ac_filefmt(log->path))
		filename = srcname;
	else
		filename = log->name;

	if (!filename)
		return -1;

	if (!strcmp(log->type, "cmd") || log->lines)
		need_timestamp = 1;

	if (need_timestamp) {
		if (get_uptime_string(timebuf, &hours) == -1)
			return -1;
		return asprintf(out, "%s/%s_%s", desdir, filename, timebuf);
	}

	return asprintf(out, "%s/%s", desdir, filename);
}

/* get_log_file_* only used to copy regular file which can be mmaped */
static void get_log_file_complete(const char *despath, const char *srcpath)
{
	const int ret = do_copy_tail(srcpath, despath, 0);

	if (ret < 0) {
		LOGE("copy (%s) failed, error (%s)\n", srcpath,
		     strerror(errno));
	}
}

static void get_log_file_tail(const char *despath, const char *srcpath,
				const int lines)
{
	char *start;
	int start_line;
	int file_lines;
	struct mm_file_t *mfile;
	int ret;

	mfile = mmap_file(srcpath);
	if (!mfile) {
		LOGE("mmap (%s) failed, error (%s)\n", srcpath,
		     strerror(errno));
		return;
	}
	file_lines = mm_count_lines(mfile);
	if (file_lines <= 0) {
		LOGW("get lines (%s, %d) failed\n", mfile->path, file_lines);
		goto unmap;
	}
	start_line = MAX(file_lines - lines, 0) + 1;
	start = mm_get_line(mfile, start_line);
	ret = overwrite_file(despath, start);
	if (ret < 0) {
		LOGE("create file with (%s, %p) failed, error (%s)\n",
		     despath, start, strerror(errno));
		goto unmap;
	}

unmap:
	unmap_file(mfile);
}

static void get_log_file(const char *despath, const char *srcpath,
			int lines)
{
	if (lines > 0)
		get_log_file_tail(despath, srcpath, lines);
	else
		get_log_file_complete(despath, srcpath);
}

static void get_log_node(const char *despath, const char *nodepath,
			size_t sizelimit)
{
	const int res = do_copy_limit(nodepath, despath, sizelimit);

	if (res < 0) {
		LOGE("copy (%s) failed, error (%s)\n", nodepath,
		     strerror(errno));
	}
}

static void get_log_cmd(const char *despath, const char *cmd)
{
	const int res = exec_out2file(despath, cmd);

	if (res)
		LOGE("get_log_by_cmd exec %s returns (%d)\n", cmd, res);
}

static void get_log_by_type(const char *despath, const struct log_t *log,
				const char *srcpath)
{
	if (!despath || !log || !srcpath)
		return;

	if (!strcmp("file", log->type)) {
		int lines;

		if (!log->lines)
			lines = 0;
		else
			if (cfg_atoi(log->lines, log->lines_len, &lines) == -1)
				return;
		get_log_file(despath, srcpath, lines);
	} else if (!strcmp("node", log->type)) {
		int size;

		if (!log->sizelimit)
			size = 0;
		else
			if (cfg_atoi(log->sizelimit, log->sizelimit_len,
				     &size) == -1)
				return;
		get_log_node(despath, log->path, (size_t)(size * 1024 * 1024));
	}
	else if (!strcmp("cmd", log->type))
		get_log_cmd(despath, log->path);
}
#ifdef HAVE_TELEMETRICS_CLIENT
static int telemd_send_data(char *payload, char *eventid, uint32_t severity,
				char *class)
{
	int res;
	struct telem_ref *handle = NULL;
	const uint32_t version = 1;

	res = tm_create_record(&handle, severity, class, version);
	if (res < 0) {
		LOGE("failed to create record: %s\n",
		     strerror(-res));
		goto fail;
	}

	/* eventid with 32 character length */
	if (eventid) {
		res = tm_set_event_id(handle, eventid);
		if (res < 0) {
			LOGE("failed to set eventid: %s\n", strerror(-res));
			goto free;
		}
	}

	res = tm_set_payload(handle, payload);
	if (res < 0) {
		LOGE("failed to set payload: %s\n", strerror(-res));
		goto free;
	}

	res = tm_send_record(handle);
	if (res < 0) {
		LOGE("failed to send record: %s\n", strerror(-res));
		goto free;
	}

	tm_free_record(handle);
	return 0;

free:
	tm_free_record(handle);
fail:
	return -1;
}

static void telemd_get_log(struct log_t *log, void *data)
{
	const struct telemd_data_t *d = (struct telemd_data_t *)data;
	char fpath[PATH_MAX];
	char *msg;
	int count;
	int len;
	int i;
	struct dirent **filelist;
	struct ac_filter_data acfd = {log->name, log->name_len};

	if (d->srcdir == NULL)
		goto send_nologs;

	/* search file which use log->name as substring */
	count = ac_scandir(d->srcdir, &filelist, filter_filename_substr,
			   &acfd, NULL);
	if (count < 0) {
		LOGE("error occurs when scanning (%s)\n", d->srcdir);
		return;
	}
	if (!count) {
		LOGE("couldn't find any files with substr (%s) under (%s)\n",
		     log->name, d->srcdir);
		goto send_nologs;
	}

	for (i = 0; i < count; i++) {
		len = snprintf(fpath, sizeof(fpath), "%s/%s", d->srcdir,
			       filelist[i]->d_name);
		free(filelist[i]);
		if (s_not_expect(len, sizeof(fpath)))
			LOGW("failed to generate path, event %s\n", d->eventid);
		else
			telemd_send_data(fpath, d->eventid,
					 d->severity, d->class);
	}

	free(filelist);

	return;

send_nologs:
	if (asprintf(&msg, "couldn't find logs with (%s), check probe's log.",
		     log->name) == -1) {
		LOGE("failed to generate msg, out of memory\n");
		return;
	}

	telemd_send_data(msg, d->eventid, d->severity, d->class);
	free(msg);
}
#endif

static void crashlog_get_log(struct log_t *log, void *data)
{

	unsigned long long start, end;
	int spent;
	int res;
	char *des;
	char *desdir = (char *)data;

	start = get_uptime();
	if (is_ac_filefmt(log->path)) {
		int i;
		char **files;
		char *name;

		const int count = config_fmt_to_files(log->path, &files);

		if (count < 0) {
			LOGE("parse config format (%s) failed\n", log->path);
			return;
		}
		if (!count) {
			LOGW("no logs found for (%s)\n", log->name);
			return;
		}

		for (i = 0; i < count; i++) {
			name = strrchr(files[i], '/') + 1;
			if (name == (char *)1) {
				LOGE("invalid path (%s) in log (%s)", files[i],
				     log->name);
				continue;
			}
			res = cal_log_filepath(&des, log, name, desdir);
			if (res == -1) {
				LOGE("cal_log_filepath failed, error (%s)\n",
				     strerror(errno));
				continue;
			}
			get_log_by_type(des, log, files[i]);
			free(des);
		}

		for (i = 0; i < count; i++)
			free(files[i]);
		free(files);
	} else {
		res = cal_log_filepath(&des, log, log->name, desdir);
		if (res == -1) {
			LOGE("cal_log_filepath failed, error (%s)\n",
			     strerror(errno));
			return;
		}
		get_log_by_type(des, log, log->path);
		free(des);
	}
	end = get_uptime();

	spent = (int)((end - start) / 1000000000LL);
	if (spent < 5)
		LOGD("get (%s) spend %ds\n", log->name, spent);
	else
		LOGW("get (%s) spend %ds\n", log->name, spent);
}

#ifdef HAVE_TELEMETRICS_CLIENT
static void telemd_send_crash(struct event_t *e, char *eventid)
{
	struct crash_t *crash;
	struct log_t *log;
	char *class;
	int id;
	int ret;
	struct telemd_data_t data = {
		.srcdir = e->dir,
		.severity = CRASH_SEVERITY,
	};

	crash = (struct crash_t *)e->private;

	ret = asprintf(&class, "clearlinux/crash/%s", crash->name);
	if (ret < 0) {
		LOGE("compute string failed, out of memory\n");
		return;
	}

	data.class = class;
	data.eventid = eventid;

	for_each_log_collect(id, log, crash) {
		if (!log)
			continue;

		log->get(log, (void *)&data);
	}
	if (!strcmp(e->channel, "inotify")) {
		char *des;
		/* get the trigger file */
		ret = asprintf(&des, "%s/%s", e->dir, e->path);
		if (ret < 0) {
			LOGE("compute string failed, out of memory\n");
			goto free_class;
		}

		if (!file_exists(des)) {
			/* find the original path */
			char *ori;

			ret = asprintf(&ori, "%s/%s", crash->trigger->path,
				       e->path);
			if (ret < 0) {
				LOGE("compute string failed, out of memory\n");
				free(des);
				goto free_class;
			}

			LOGW("(%s) unavailable, try the original path (%s)\n",
			     des, ori);
			if (!file_exists(ori)) {
				LOGE("original path (%s) is unavailable\n",
				     ori);
			} else {
				telemd_send_data(ori, eventid, CRASH_SEVERITY,
						 class);
			}

			free(ori);
		} else {
			telemd_send_data(des, eventid, CRASH_SEVERITY, class);
		}

		free(des);
	}
free_class:
	free(class);
}

static void telemd_send_info(struct event_t *e, char *eventid)
{
	struct info_t *info;
	struct log_t *log;
	char *class;
	int id;
	int ret;
	struct telemd_data_t data = {
		.srcdir = e->dir,
		.severity = INFO_SEVERITY,
	};

	info = (struct info_t *)e->private;
	ret = asprintf(&class, "clearlinux/info/%s", info->name);
	if (ret < 0) {
		LOGE("compute string failed, out of memory\n");
		return;
	}

	data.class = class;
	data.eventid = eventid;

	for_each_log_collect(id, log, info) {
		if (!log)
			continue;

		log->get(log, (void *)&data);
	}

	free(class);
}

static void telemd_send_uptime(void)
{
	struct sender_t *telemd = get_sender_by_name("telemd");
	struct uptime_t *uptime;
	char *class;
	char boot_time[UPTIME_SIZE];
	int hours;
	int ret;
	static int uptime_hours;
	static int loop_uptime_event = 1;

	if (!telemd)
		return;

	ret = get_uptime_string(boot_time, &hours);
	if (ret < 0) {
		LOGE("cannot get uptime - %s\n", strerror(-ret));
		return;
	}
	uptime = telemd->uptime;
	if (cfg_atoi(uptime->eventhours, uptime->eventhours_len,
		     &uptime_hours) == -1)
		return;

	if (hours / uptime_hours >= loop_uptime_event) {
		char *content;

		loop_uptime_event = (hours / uptime_hours) + 1;
		ret = asprintf(&class, "clearlinux/uptime/%s", boot_time);
		if (ret < 0) {
			LOGE("compute string failed, out of memory\n");
			return;
		}

		ret = asprintf(&content, "system boot time: %s", boot_time);
		if (ret < 0) {
			LOGE("compute string failed, out of memory\n");
			free(class);
			return;
		}

		telemd_send_data(content, NULL, INFO_SEVERITY, class);
		free(class);
		free(content);
	}
}

static void telemd_send_reboot(char *eventid)
{
	struct sender_t *telemd = get_sender_by_name("telemd");
	char *class;
	char reason[REBOOT_REASON_SIZE];
	int ret;

	if (!telemd)
		return;

	if (swupdated(telemd)) {
		char *content;

		ret = asprintf(&class, "clearlinux/swupdate/-");
		if (ret < 0) {
			LOGE("compute string failed, out of memory\n");
			return;
		}

		ret = asprintf(&content, "system update to: %s",
			 gbuildversion);
		if (ret < 0) {
			LOGE("compute string failed, out of memory\n");
			free(class);
			return;
		}

		telemd_send_data(content, NULL, INFO_SEVERITY, class);
		free(class);
		free(content);
	}

	read_startupreason(reason, sizeof(reason));
	ret = asprintf(&class, "clearlinux/reboot/%s", reason);
	if (ret < 0) {
		LOGE("compute string failed, out of memory\n");
		return;
	}

	telemd_send_data("reboot", eventid, INFO_SEVERITY, class);
	free(class);
}

static void telemd_send_vmevent(struct event_t *e, char *eventid, char *data,
				size_t dlen)
{
	char *vmkey;
	char *event;
	char *type;
	char *rest;
	size_t klen;
	size_t elen;
	size_t tlen;
	size_t rlen;
	char *vmlogpath = NULL;
	char *class;
	char *files[512];
	int count;
	int i;
	uint32_t severity;
	char *log;
	int res;
	struct vm_event_t *vme;

	vmkey = strings_ind(data, dlen, 0, &klen);
	event = strings_ind(data, dlen, 1, &elen);
	type = strings_ind(data, dlen, 2, &tlen);
	rest = strings_ind(data, dlen, 3, &rlen);
	if (!vmkey || !event || !type || !rest)
		return;

	if (strcmp(event, "CRASH") == 0)
		severity = CRASH_SEVERITY;
	else
		severity = INFO_SEVERITY;

	/* if line contains log, fill vmlogpath */
	log = strstr(rest, ANDROID_LOGS_DIR);
	if (log) {
		const char *logf;

		if (!e->dir)
			return;
		logf = log + sizeof(ANDROID_LOGS_DIR) - 1;
		if (asprintf(&vmlogpath, "%s/%s", e->dir, logf) == -1)
			return;
	}

	vme = (struct vm_event_t *)e->private;
	res = asprintf(&class, "%s/%s/%s", vme->vm->name, event, type);
	if (res < 0) {
		LOGE("compute string failed, out of memory\n");
		goto free_vmlogpath;
	}

	if (!vmlogpath) {
		telemd_send_data("vm event doesn't contain logs", eventid,
				 severity, class);
		goto free_class;
	}

	/* send logs */
	count = lsdir(vmlogpath, files, ARRAY_SIZE(files));
	if (count > 2) {
		for (i = 0; i < count; i++) {
			if (!strstr(files[i], "/.") &&
			    !strstr(files[i], "/..")) {
				telemd_send_data(files[i], eventid, severity,
						 class);
			}
		}
	} else if (count < 0) {
		LOGE("lsdir (%s) failed, error (%s)\n", vmlogpath,
		     strerror(-count));
	} else {
		LOGE("get (%d) files in (%s) ???\n", count, vmlogpath);
	}

	while (count > 0)
		free(files[--count]);

free_class:
	free(class);
free_vmlogpath:
	if (vmlogpath)
		free(vmlogpath);

	return;
}

static int telemd_new_event(struct event_t *e, char *result, size_t rsize,
				char **eid)
{
	struct crash_t *crash;
	struct info_t *info;
	struct vm_event_t *vme;
	char *key;
	const char *e_subtype = NULL;
	size_t e_subtype_len = 0;
	const char *estr = etype_str[e->event_type];
	size_t eslen = strlen(etype_str[e->event_type]);

	switch (e->event_type) {
	case CRASH:
		crash = (struct crash_t *)e->private;
		e_subtype = crash->name;
		e_subtype_len = crash->name_len;
		break;
	case INFO:
		info = (struct info_t *)e->private;
		e_subtype = info->name;
		e_subtype_len = info->name_len;
		break;
	case UPTIME:
		return 0;
	case REBOOT:
		break;
	case VM:
		vme = (struct vm_event_t *)e->private;
		estr = vme->vm->name;
		eslen = vme->vm->name_len;
		e_subtype = strings_ind(result, rsize, 2, &e_subtype_len);
		if (!e_subtype)
			return -1;
		break;
	default:
		break;
	}

	key = generate_event_id(estr, eslen, e_subtype, e_subtype_len,
				KEY_LONG);
	if (!key) {
		LOGE("failed to generate event id, %s\n", strerror(errno));
		return -1;
	}
	*eid = key;
	return 0;
}

static int telemd_event_analyze(struct event_t *e, char **result,
				size_t *rsize)
{
	if (e->event_type == VM) {
		struct vm_event_t *vme = (struct vm_event_t *)e->private;

		if (android_event_analyze(vme->vm_msg, vme->vm_msg_len,
					  result, rsize) == -1) {
			LOGE("failed to analyze android event\n");
			return -1;
		}
	}
	return 0;
}

static void telemd_send(struct event_t *e)
{
	int id;
	struct log_t *log;
	size_t rsize;
	char *result = NULL;
	char *eid = NULL;

	if (telemd_event_analyze(e, &result, &rsize) == -1) {
		LOGE("failed to analyze event\n");
		return;
	}
	if (telemd_new_event(e, result, rsize, &eid) == -1) {
		LOGE("failed to request resouce\n");
		if (result)
			free(result);
		return;
	}
	for_each_log(id, log, conf) {
		if (!log)
			continue;

		log->get = telemd_get_log;
	}

	switch (e->event_type) {
	case CRASH:
		telemd_send_crash(e, eid);
		break;
	case INFO:
		telemd_send_info(e, eid);
		break;
	case UPTIME:
		telemd_send_uptime();
		break;
	case REBOOT:
		telemd_send_reboot(eid);
		break;
	case VM:
		telemd_send_vmevent(e, eid, result, rsize);
		break;
	default:
		LOGE("unsupoorted event type %d\n", e->event_type);
	}
	if (eid)
		free(eid);
	if (result)
		free(result);
}
#endif

static void crashlog_send_crash(struct event_t *e, char *eid,
				char *data, size_t dlen)
{
	char *data0;
	char *data1;
	char *data2;
	size_t d0len;
	size_t d1len;
	size_t d2len;
	struct crash_t *crash = (struct crash_t *)e->private;
	int id;
	struct log_t *log;

	hist_raise_event(etype_str[e->event_type], crash->name, e->dir, "",
			 eid);
	if (!e->dir)
		return;

	data0 = strings_ind(data, dlen, 0, &d0len);
	data1 = strings_ind(data, dlen, 1, &d1len);
	data2 = strings_ind(data, dlen, 2, &d2len);
	if (!data1 || !data1 || !data2)
		return;

	generate_crashfile(e->dir, etype_str[e->event_type],
			   strlen(etype_str[e->event_type]), eid,
			   SHORT_KEY_LENGTH, crash->name, crash->name_len,
			   data0, d0len, data1, d1len, data2, d2len);

	for_each_log_collect(id, log, crash) {
		if (!log)
			continue;
		log->get(log, (void *)e->dir);
	}
	if (!strcmp(e->channel, "inotify")) {
		/* get the trigger file */
		char *src;
		char *des;

		if (asprintf(&des, "%s/%s", e->dir, e->path) == -1) {
			LOGE("out of memory\n");
			return;
		}

		if (asprintf(&src, "%s/%s", crash->trigger->path,
			     e->path) == -1) {
			LOGE("out of memory\n");
			free(des);
			return;
		}

		if (do_copy_tail(src, des, 0) < 0)
			LOGE("failed to copy (%s) to (%s)\n", src, des);

		free(src);
		free(des);
	}
}

static void crashlog_send_info(struct event_t *e, char *eid)
{
	int id;
	struct info_t *info = (struct info_t *)e->private;
	struct log_t *log;

	hist_raise_event(etype_str[e->event_type], info->name, e->dir, "", eid);
	if (!e->dir)
		return;
	for_each_log_collect(id, log, info) {
		if (!log)
			continue;
		log->get(log, (void *)e->dir);
	}
}

static void crashlog_send_uptime(void)
{
	hist_raise_uptime(NULL);
}

static void crashlog_send_reboot(struct event_t *e, char *eid)
{
	char reason[REBOOT_REASON_SIZE];
	char *key;
	struct sender_t *crashlog;

	crashlog = get_sender_by_name("crashlog");
	if (!crashlog)
		return;

	if (swupdated(crashlog)) {
		key = generate_event_id("INFO", 4, "SWUPDATE", 8, KEY_SHORT);
		if (key == NULL) {
			LOGE("generate event id failed, error (%s)\n",
			     strerror(errno));
			return;
		}

		hist_raise_event("INFO", "SWUPDATE", NULL, "", key);
		free(key);
	}

	read_startupreason(reason, sizeof(reason));
	hist_raise_event(etype_str[e->event_type], reason, NULL, "", eid);
}

static void crashlog_send_vmevent(struct event_t *e, char *eid,
				char *data, size_t dlen)
{
	char *vmkey;
	char *event;
	char *type;
	char *rest;
	size_t klen;
	size_t elen;
	size_t tlen;
	size_t rlen;
	char *vmlogpath;
	char *log;
	int res;
	int cnt;
	ext2_filsys datafs;
	struct sender_t *crashlog = get_sender_by_name("crashlog");
	struct vm_event_t *vme = (struct vm_event_t *)e->private;
	enum vmrecord_mark_t mark = SUCCESS;

	if (!crashlog)
		return;

	vmkey = strings_ind(data, dlen, 0, &klen);
	event = strings_ind(data, dlen, 1, &elen);
	type = strings_ind(data, dlen, 2, &tlen);
	rest = strings_ind(data, dlen, 3, &rlen);
	if (!vmkey || !event || !type || !rest)
		return;

	hist_raise_event(vme->vm->name, type, e->dir, "", eid);

	if (!e->dir)
		goto mark_record;

	generate_crashfile(e->dir, event, elen, eid, SHORT_KEY_LENGTH, type,
			   tlen, vme->vm->name, vme->vm->name_len, vmkey, klen,
			   NULL, 0);

	log = strstr(rest, ANDROID_LOGS_DIR);
	if (!log)
		goto mark_record;

	/* if line contains log, we need dump each file in the logdir */
	vmlogpath = log + 1;

	if (e2fs_open(loop_dev, &datafs) == -1) {
		mark = WAITING_SYNC;
		goto mark_record;
	}

	res = e2fs_dump_dir_by_dpath(datafs, vmlogpath, e->dir, &cnt);
	e2fs_close(datafs);
	if (res == -1) {
		if (cnt) {
			LOGE("dump (%s) abort at (%d)\n", vmlogpath, cnt);
			mark = WAITING_SYNC;
		} else {
			LOGW("(%s) doesn't exsit\n", vmlogpath);
			mark = MISS_LOG;
		}
	}
	if (cnt == 1) {
		LOGW("%s is empty, will sync it in the next loop\n", vmlogpath);
		mark = WAITING_SYNC;
	}
	if (res == -1 || cnt == -1) {
		if (remove_r(e->dir) == -1)
			LOGE("failed to remove %s, %s\n", e->dir,
			     strerror(errno));
		free(e->dir);
		e->dir = NULL;
	}

mark_record:
	vmrecord_open_mark(&crashlog->vmrecord, vmkey, klen, mark);
	return;
}

static int crashlog_event_analyze(struct event_t *e, char **result,
				size_t *rsize)
{
	struct crash_t *rcrash;
	struct crash_t *crash;
	char *trfile = NULL;
	struct vm_event_t *vme;

	switch (e->event_type) {
	case CRASH:
		rcrash = (struct crash_t *)e->private;
		if (!strcmp(rcrash->trigger->type, "dir")) {
			if (asprintf(&trfile, "%s/%s", rcrash->trigger->path,
				     e->path) == -1) {
				LOGE("failed to asprintf\n");
				return -1;
			}
		}
		crash = rcrash->reclassify(rcrash, trfile, result, rsize);
		if (trfile)
			free(trfile);
		if (!crash) {
			LOGE("failed to reclassify (%s)\n", rcrash->name);
			return -1;
		}
		/* change the class */
		e->private = (void *)crash;
		break;
	case VM:
		vme = (struct vm_event_t *)e->private;
		if (android_event_analyze(vme->vm_msg, vme->vm_msg_len,
					  result, rsize) == -1) {
			LOGE("failed to analyze android event\n");
			return -1;
		}
		break;
	default:
		break;
	}
	return 0;
}

static int crashlog_new_event(struct event_t *e, char *result, size_t rsize,
				char **eid)
{
	char *key;
	const char *e_subtype = NULL;
	size_t e_subtype_len = 0;
	struct crash_t *crash;
	struct info_t *info;
	struct vm_event_t *vme;
	enum e_dir_mode mode;
	int need_logs = 0;
	struct sender_t *crashlog;
	char *vmkey;
	size_t vklen;
	const char *estr = etype_str[e->event_type];
	size_t eslen = strlen(etype_str[e->event_type]);

	switch (e->event_type) {
	case CRASH:
		crash = (struct crash_t *)e->private;
		e_subtype = crash->name;
		e_subtype_len = crash->name_len;
		if (to_collect_logs(crash) || !strcmp(e->channel, "inotify")) {
			need_logs = 1;
			mode = MODE_CRASH;
		}
		break;
	case INFO:
		info = (struct info_t *)e->private;
		e_subtype = info->name;
		e_subtype_len = info->name_len;
		if (to_collect_logs(info) || !strcmp(e->channel, "inotify")) {
			need_logs = 1;
			mode = MODE_STATS;
		}
		break;
	case UPTIME:
		return 0;
	case REBOOT:
		break;
	case VM:
		vme = (struct vm_event_t *)e->private;
		estr = vme->vm->name;
		eslen = vme->vm->name_len;
		e_subtype = strings_ind(result, rsize, 2, &e_subtype_len);
		if (!e_subtype)
			return -1;
		need_logs = 1;
		mode = MODE_VMEVENT;
		break;
	default:
		break;
	}

	key = generate_event_id(estr, eslen, e_subtype, e_subtype_len,
				KEY_SHORT);
	if (!key) {
		LOGE("failed to generate event id, %s\n", strerror(errno));
		goto fail;
	}

	if (!need_logs) {
		*eid = key;
		return 0;
	}

	if (crashlog_check_space() == -1) {
		hist_raise_event(estr, e_subtype, "SPACE_FULL", "", key);
		free(key);
		goto fail;
	}

	e->dir = generate_log_dir(mode, key, &e->dlen);
	if (!e->dir) {
		LOGE("failed to generate crashlog dir\n");
		free(key);
		goto fail;
	}
	*eid = key;
	return 0;
fail:
	if (e->event_type == VM) {
		crashlog = get_sender_by_name("crashlog");
		vmkey = strings_ind(result, rsize, 0, &vklen);
		if (!crashlog || !vmkey)
			return -1;
		vmrecord_open_mark(&crashlog->vmrecord, vmkey, vklen, NO_RESRC);
	}
	return -1;
}

static void crashlog_send(struct event_t *e)
{

	int id;
	struct log_t *log;
	size_t rsize = 0;
	char *result = NULL;
	char *eid = NULL;

	if (crashlog_event_analyze(e, &result, &rsize) == -1) {
		LOGE("failed to analyze event\n");
		return;
	}
	if (crashlog_new_event(e, result, rsize, &eid) == -1) {
		LOGE("failed to request resouce\n");
		if (result)
			free(result);
		return;
	}
	for_each_log(id, log, conf) {
		if (!log)
			continue;

		log->get = crashlog_get_log;
	}
	switch (e->event_type) {
	case CRASH:
		crashlog_send_crash(e, eid, result, rsize);
		break;
	case INFO:
		crashlog_send_info(e, eid);
		break;
	case UPTIME:
		crashlog_send_uptime();
		break;
	case REBOOT:
		crashlog_send_reboot(e, eid);
		break;
	case VM:
		crashlog_send_vmevent(e, eid, result, rsize);
		break;
	default:
		LOGE("unsupoorted event type %d\n", e->event_type);
	}
	if (eid)
		free(eid);
	if (result)
		free(result);
}

int init_sender(void)
{
	int id;
	int fd;
	struct sender_t *sender;
	struct uptime_t *uptime;

	for_each_sender(id, sender, conf) {
		if (!sender)
			continue;

		if (!directory_exists(sender->outdir))
			if (mkdir_p(sender->outdir) < 0) {
				LOGE("mkdir (%s) failed, error (%s)\n",
				     sender->outdir, strerror(errno));
				return -1;
			}

		if (init_properties(sender)) {
			LOGE("init sender failed\n");
			return -1;
		}

		/* touch uptime file, to add inotify */
		uptime = sender->uptime;
		if (uptime) {
			fd = open(uptime->path, O_RDWR | O_CREAT, 0666);
			if (fd < 0) {
				LOGE("failed to open (%s), error (%s)\n",
				     uptime->path, strerror(errno));
				return -1;
			}
			close(fd);
		}

		if (!strcmp(sender->name, "crashlog")) {
			sender->send = crashlog_send;
			if (prepare_history())
				return -1;
			if (asprintf(&sender->vmrecord.path,
				     "%s/VM_eventsID.log",
				       sender->outdir) == -1) {
				LOGE("failed to asprintf\n");
				return -1;
			}
			pthread_mutex_init(&sender->vmrecord.mtx, NULL);

#ifdef HAVE_TELEMETRICS_CLIENT
		} else if (!strcmp(sender->name, "telemd")) {
			sender->send = telemd_send;
#endif
		}
	}

	return 0;
}
