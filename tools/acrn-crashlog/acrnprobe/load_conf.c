/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include "load_conf.h"
#include "event_queue.h"
#include "log_sys.h"
#include "strutils.h"

static void print(void)
{
	int id, id2;
	int i, j;
	struct sender_t *sender;
	struct trigger_t *trigger;
	struct vm_t *vm;
	struct log_t *log;
	struct info_t *info;
	struct crash_t *crash;
	struct crash_t *crash_tmp;

#define print_id_item(item, root, id) \
		LOGD("%-8s(%d): %-15s:(%s)\n", #root, id, #item, root->item)
#define print_2id_item(item, root, id, tid) \
		LOGD("%-8s(%d): %-15s(%d):(%s)\n", \
		     #root, id, #item, tid, root->item[tid])
	for_each_sender(id, sender, conf) {
		if (!sender)
			continue;
		print_id_item(name, sender, id);
		print_id_item(outdir, sender, id);
		print_id_item(maxcrashdirs, sender, id);
		print_id_item(maxlines, sender, id);
		print_id_item(spacequota, sender, id);
		print_id_item(foldersize, sender, id);

		if (sender->uptime) {
			print_id_item(uptime->name, sender, id);
			print_id_item(uptime->path, sender, id);
			print_id_item(uptime->frequency, sender, id);
			print_id_item(uptime->eventhours, sender, id);
		}
	}

	for_each_trigger(id, trigger, conf) {
		if (!trigger)
			continue;
		print_id_item(name, trigger, id);
		print_id_item(type, trigger, id);
		print_id_item(path, trigger, id);
	}

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;
		print_id_item(name, vm, id);
		print_id_item(channel, vm, id);
		print_id_item(interval, vm, id);
		for (i = 0; i < VM_EVENT_TYPE_MAX; i++) {
			if (vm->syncevent[i])
				print_2id_item(syncevent, vm, id, i);
		}
	}

	for_each_log(id, log, conf) {
		if (!log)
			continue;
		print_id_item(name, log, id);
		print_id_item(type, log, id);
		print_id_item(lines, log, id);
		print_id_item(path, log, id);
	}

	for_each_info(id, info, conf) {
		if (!info)
			continue;
		print_id_item(name, info, id);
		print_id_item(channel, info, id);
		print_id_item(interval, info, id);
		print_id_item(trigger->name, info, id);
		for_each_log_collect(id2, log, info) {
			if (!log)
				continue;
			LOGD("%-8s(%d): %-15s(%c):(%s)\n",
			     "info", id, "log", 'x', log->name);
		}
	}

	for_each_crash(id, crash, conf) {
		char buf[512];
		char *tail;
		int len;

		if (!crash)
			continue;

		print_id_item(name, crash, id);
		memset(buf, 0, sizeof(buf));
		LOGD("%-8s(%d): properties: %s, %s\n", "crash", id,
		     is_root_crash(crash) ? "root" : "non-root",
		     is_leaf_crash(crash) ? "leaf" : "non-leaf");
		len = snprintf(buf, sizeof(buf), "%-8s(%d): children: ",
			     "crash", id);
		if (s_not_expect(len, sizeof(buf))) {
			LOGE("failed to construct the children of crash\n");
			continue;
		}
		tail = buf + len;
		for_crash_children(crash_tmp, crash) {
			if (len + crash_tmp->name_len + 2 >= sizeof(buf)) {
				LOGE("names of children too long - truncate\n");
				break;;
			}
			tail = mempcpy(tail, crash_tmp->name,
				       crash_tmp->name_len);
			*tail++ = ' ';
			len += crash_tmp->name_len + 1;
		}
		*tail = '\0';
		LOGD("%s\n", buf);
		print_id_item(trigger->name, crash, id);
		print_id_item(channel, crash, id);
		print_id_item(interval, crash, id);
		for (i = 0; i < CONTENT_MAX; i++)
			if (crash->content[i])
				print_2id_item(content, crash, id, i);

		for (i = 0; i < EXPRESSION_MAX; i++)
			for (j = 0; j < CONTENT_MAX; j++)
				if (crash->mightcontent[i][j])
					LOGD("%-8s(%d): %-15s(%d,%d):(%s)\n",
					     "crash", id, "mightcontent", i, j,
					     crash->mightcontent[i][j]);

		for (i = 0; i < DATA_MAX; i++)
			if (crash->data[i])
				print_2id_item(data, crash, id, i);
	}
}

static int get_prop_int(xmlNodePtr cur, const char *key, const int max)
{
	xmlChar *prop;
	int value;

	prop = xmlGetProp(cur, (const xmlChar *)key);
	if (!prop) {
		LOGE("get prop (%s) failed\n", key);
		return -1;
	}

	if (cfg_atoi((const char *)prop, xmlStrlen(prop), &value) == -1)
		return -1;

	xmlFree(prop);

	if (value > max) {
		LOGE("prop (%s) exceeds MAX (%d)\n", prop, max);
		return -1;
	}

	return value;
}

static int get_id_index(xmlNodePtr cur, const int max)
{
	int value = get_prop_int(cur, "id", max);

	if (value <= 0)
		return -1;

	/* array index = value - 1 */
	return value - 1;
}

static int get_expid_index(xmlNodePtr cur, const int max)
{
	int value = get_prop_int(cur, "expression", max);

	if (value <= 0)
		return -1;

	/* array index = value - 1 */
	return value - 1;
}

#define load_cur_content(cur, type, mem) \
(__extension__ \
({ \
		xmlChar *load##mem; \
		int _ret = -1; \
		load##mem = xmlNodeGetContent(cur); \
		if (load##mem) { \
			type->mem = (const char *)load##mem; \
			type->mem##_len = xmlStrlen(load##mem); \
			_ret = 0; \
		} \
		_ret; \
}) \
)

#define load_cur_content_with_id(cur, type, mem, max) \
(__extension__ \
({ \
		xmlChar *load##mem; \
		int index; \
		int _ret = -1; \
		load##mem = xmlNodeGetContent(cur); \
		if (load##mem) { \
			index = get_id_index(cur, max); \
			if (index != -1) { \
				type->mem[index] = (const char *)load##mem; \
				type->mem##_len[index] = xmlStrlen(load##mem); \
				_ret = 0; \
			} \
		} \
		_ret; \
}) \
)

#define load_trigger(cur, event) \
(__extension__ \
({ \
		int _ret = -1; \
		xmlChar *content = xmlNodeGetContent(cur); \
		if (content) { \
			event->trigger = \
				get_trigger_by_name((const char *)content); \
			xmlFree(content); \
			_ret = 0; \
		} \
		_ret; \
}) \
)

struct crash_t *get_crash_by_wd(int wd)
{
	int id;
	struct crash_t *crash;

	for_each_crash(id, crash, conf) {
		if (!crash)
			continue;

		if (crash->wd == wd)
			return crash;
	}
	return NULL;
}

struct uptime_t *get_uptime_by_wd(int wd)
{
	int id;
	struct uptime_t  *ut;
	struct sender_t *sender;

	for_each_sender(id, sender, conf) {
		if (!sender)
			continue;

		ut = sender->uptime;
		if (ut->wd == wd)
			return ut;
	}

	return NULL;
}

enum event_type_t get_conf_by_wd(int wd, void **private)
{
	void *conf;

	conf = (void *)get_uptime_by_wd(wd);
	if (conf) {
		*private = conf;
		return UPTIME;
	}

	conf = (void *)get_crash_by_wd(wd);
	if (conf) {
		*private = conf;
		return CRASH;
	}

	return UNKNOWN;

}

int sender_id(const struct sender_t *s)
{
	int id;
	struct sender_t *sender;

	for_each_sender(id, sender, conf) {
		if (!sender)
			continue;

		if (s == sender)
			return id;
	}

	return -1;
}

struct sender_t *get_sender_by_name(const char *name)
{
	int id;
	struct sender_t *sender;

	for_each_sender(id, sender, conf) {
		if (!sender)
			continue;

		if (strcmp(name, sender->name) == 0)
			return sender;
	}
	return NULL;
}

struct trigger_t *get_trigger_by_name(const char *name)
{
	int id;
	struct trigger_t *trigger;

	for_each_trigger(id, trigger, conf) {
		if (!trigger)
			continue;
		if (strcmp(name, trigger->name) == 0)
			return trigger;
	}
	return NULL;
}

struct log_t *get_log_by_name(const char *name)
{
	int id;
	struct log_t *log;

	for_each_log(id, log, conf) {
		if (!log)
			continue;
		if (strcmp(name, log->name) == 0)
			return log;
	}
	return NULL;
}

struct vm_t *get_vm_by_name(const char *name)
{
	int id;
	struct vm_t *vm;

	for_each_vm(id, vm, conf) {
		if (!vm)
			continue;
		if (strcmp(name, vm->name) == 0)
			return vm;
	}
	return NULL;
}

int crash_depth(struct crash_t *tcrash)
{
	int id;
	int level = 0;
	struct crash_t *crash;

	for_each_crash(id, crash, conf) {
		if (!crash)
			continue;

		if (crash->channel == tcrash->channel &&
		    crash->trigger == tcrash->trigger &&
		    crash->level > level)
			level = crash->level;
	}
	level = level - tcrash->level;
	return level;
}

int cfg_atoi(const char *a, size_t alen, int *i)
{
	char *eptr;
	int res;

	if (!a || !alen || !i)
		return -1;

	res = (int)strtol(a, &eptr, 0);
	if (a + alen != eptr) {
		LOGE("Failed to convert (%s) to type int, check config file\n",
		     a);
		return -1;
	}

	*i = res;
	return 0;
}

static int is_enable(xmlNodePtr cur)
{
	xmlChar *prop;
	int ret = 0;

	prop = xmlGetProp(cur, (const xmlChar *)"enable");
	if (prop) {
		ret = !xmlStrcmp((const xmlChar *)"true", prop);
		xmlFree(prop);
	}

	return ret;
}

#define name_is(cur, key) \
		(xmlStrcmp(cur->name, (const xmlChar *)key) == 0)

static int parse_info(xmlNodePtr cur, struct info_t *info)
{
	int id;
	int res = 0;
	xmlChar *content;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, info, name);
		else if (name_is(cur, "trigger"))
			load_trigger(cur, info);
		else if (name_is(cur, "channel"))
			res = load_cur_content(cur, info, channel);
		else if (name_is(cur, "log")) {
			id = get_id_index(cur, LOG_MAX);
			if (id == -1)
				return -1;
			content = xmlNodeGetContent(cur);
			if (!content)
				return -1;
			info->log[id] = get_log_by_name((const char *)content);
			xmlFree(content);
		}

		if (res)
			return -1;

		cur = cur->next;
	}

	return 0;
}

static int parse_log(xmlNodePtr cur, struct log_t *log)
{
	int res = 0;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, log, name);
		else if (name_is(cur, "type"))
			res = load_cur_content(cur, log, type);
		else if (name_is(cur, "path"))
			res = load_cur_content(cur, log, path);
		else if (name_is(cur, "lines"))
			res = load_cur_content(cur, log, lines);

		if (res)
			return -1;

		cur = cur->next;
	}

	return 0;
}

static int parse_crash(xmlNodePtr cur, struct crash_t *crash)
{

	int id;
	int expid;
	int res = 0;
	xmlChar *content;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, crash, name);
		else if (name_is(cur, "trigger"))
			load_trigger(cur, crash);
		else if (name_is(cur, "channel"))
			res = load_cur_content(cur, crash, channel);
		else if (name_is(cur, "content"))
			res = load_cur_content_with_id(cur, crash,
						       content, CONTENT_MAX);
		else if (name_is(cur, "log")) {
			id = get_id_index(cur, LOG_MAX);
			if (id == -1)
				return -1;

			content = xmlNodeGetContent(cur);
			if (!content)
				return -1;
			crash->log[id] = get_log_by_name((const char *)content);
			xmlFree(content);
		} else if (name_is(cur, "data"))
			res = load_cur_content_with_id(cur, crash,
						       data, DATA_MAX);
		else if (name_is(cur, "mightcontent")) {
			id = get_id_index(cur, CONTENT_MAX);
			expid = get_expid_index(cur, EXPRESSION_MAX);
			if (id == -1 || expid == -1)
				return -1;

			content = xmlNodeGetContent(cur);
			if (!content)
				return -1;
			crash->mightcontent[expid][id] = (const char *)content;
			crash->mightcontent_len[expid][id] = xmlStrlen(content);
		}

		if (res)
			return -1;

		cur = cur->next;
	}

	return 0;
}

static int parse_crashes(xmlNodePtr crashes)
{
	int id;
	int res;
	xmlNodePtr cur;
	struct crash_t *crash;

	cur = crashes->xmlChildrenNode;

	while (cur) {
		if (is_enable(cur)) {
			crash = malloc(sizeof(*crash));
			if (!crash)
				return -1;

			res = get_prop_int(cur, "inherit", CRASH_MAX);
			if (res < 0) {
				free(crash);
				return -1;
			}

			id = res - 1;
			if (id >= 0) {
				memcpy(crash, conf.crash[id], sizeof(*crash));
				crash->parents = conf.crash[id];
				crash->level++;
				TAILQ_INSERT_TAIL(&crash->parents->children,
						crash, entries);
			} else {
				memset(crash, 0, sizeof(*crash));
			}
			id = get_id_index(cur, CRASH_MAX);
			if (id == -1) {
				free(crash);
				return -1;
			}
			res = parse_crash(cur, crash);
			if (res) {
				free(crash);
				return -1;
			}

			TAILQ_INIT(&crash->children);
			conf.crash[id] = crash;
		}
		cur = cur->next;
	}

	return 0;
}

static int parse_vm(xmlNodePtr cur, struct vm_t *vm)
{
	int res = 0;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, vm, name);
		else if (name_is(cur, "channel"))
			res = load_cur_content(cur, vm, channel);
		else if (name_is(cur, "interval"))
			res = load_cur_content(cur, vm, interval);
		else if (name_is(cur, "syncevent"))
			res = load_cur_content_with_id(cur, vm,
						       syncevent,
						       VM_EVENT_TYPE_MAX);

		if (res)
			return -1;

		cur = cur->next;
	}

	return 0;
}

static int parse_uptime(xmlNodePtr cur, struct sender_t *sender)
{
	struct uptime_t *uptime;
	int res = 0;

	uptime = malloc(sizeof(*uptime));
	if (!uptime)
		return -1;

	memset(uptime, 0, sizeof(*uptime));
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, uptime, name);
		else if (name_is(cur, "frequency"))
			res = load_cur_content(cur, uptime, frequency);
		else if (name_is(cur, "eventhours"))
			res = load_cur_content(cur, uptime, eventhours);

		if (res)
			return -1;

		cur = cur->next;
	}
	res = asprintf(&uptime->path, "%s/uptime", sender->outdir);
	if (res < 0) {
		LOGE("build string failed\n");
		return -1;
	}
	sender->uptime = uptime;

	return 0;
}

static int parse_trigger(xmlNodePtr cur, struct trigger_t *trigger)
{
	int res = 0;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, trigger, name);
		else if (name_is(cur, "type"))
			res = load_cur_content(cur, trigger, type);
		else if (name_is(cur, "path"))
			res = load_cur_content(cur, trigger, path);

		if (res)
			return -1;

		cur = cur->next;
	}

	return 0;
}

static int parse_sender(xmlNodePtr cur, struct sender_t *sender)
{
	int res = 0;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			res = load_cur_content(cur, sender, name);
		else if (name_is(cur, "outdir"))
			res = load_cur_content(cur, sender, outdir);
		else if (name_is(cur, "maxcrashdirs"))
			res = load_cur_content(cur, sender, maxcrashdirs);
		else if (name_is(cur, "maxlines"))
			res = load_cur_content(cur, sender, maxlines);
		else if (name_is(cur, "spacequota"))
			res = load_cur_content(cur, sender, spacequota);
		else if (name_is(cur, "foldersize"))
			res = load_cur_content(cur, sender, foldersize);
		else if (name_is(cur, "uptime"))
			res = parse_uptime(cur, sender);

		if (res)
			return -1;

		cur = cur->next;
	}

	return 0;
}

#define common_parse(node, mem, maxmem) \
(__extension__ \
({ \
	int id; \
	int _ret = 0; \
	int res; \
	struct mem##_t *mem; \
\
	node = node->xmlChildrenNode; \
\
	while (node) { \
		if (is_enable(node)) { \
			id = get_id_index(node, maxmem); \
			if (id < 0) { \
				_ret = -1; \
				break; \
			} \
\
			mem = malloc(sizeof(*mem)); \
			if (!mem) { \
				_ret = -1; \
				break; \
			} \
			memset(mem, 0, sizeof(*mem)); \
			conf.mem[id] = mem; \
			res = parse_##mem(node, mem); \
			if (res) { \
				free(mem); \
				_ret = -1; \
				break; \
			} \
		} \
		node = node->next; \
	} \
	_ret; \
}) \
)

int load_conf(const char *path)
{
	int res = 0;
	xmlDocPtr doc;
	xmlNodePtr cur, node;

	doc = xmlParseFile(path);
	if (!doc) {
		LOGI("Parsing conf (%s) fail\n", path);
		goto error;
	}

	cur = xmlDocGetRootElement(doc);
	if (!cur) {
		LOGE("Get root (%s) fail\n", path);
		goto free;
	}

	cur = cur->xmlChildrenNode;
	while ((node = cur)) {
		if (name_is(node, "senders"))
			res = common_parse(node, sender, SENDER_MAX);
		else if (name_is(node, "triggers"))
			res = common_parse(node, trigger, TRIGGER_MAX);
		else if (name_is(node, "vms"))
			res = common_parse(node, vm, VM_MAX);
		else if (name_is(node, "crashes"))
			res = parse_crashes(node);
		else if (name_is(node, "logs"))
			res = common_parse(node, log, LOG_MAX);
		else if (name_is(node, "infos"))
			res = common_parse(node, info, INFO_MAX);

		if (res)
			goto free;

		cur = cur->next;
	}
	print();
	xmlFreeDoc(doc);
	return 0;
free:
	xmlFreeDoc(doc);
error:
	return -1;
}
