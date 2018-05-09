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
	char buf[512];

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
		if (!crash)
			continue;

		print_id_item(name, crash, id);
		memset(buf, 0, sizeof(*buf));
		LOGD("%-8s(%d): properties: %s, %s\n", "crash", id,
		     is_root_crash(crash) ? "root" : "non-root",
		     is_leaf_crash(crash) ? "leaf" : "non-leaf");
		sprintf(buf + strlen(buf), "%-8s(%d): children: ", "crash", id);
		for_crash_children(crash_tmp, crash)
			sprintf(buf + strlen(buf), "%s ", crash_tmp->name);
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

static int get_prop_int(xmlNodePtr cur, char *key)
{
	xmlChar *prop;
	int ret = 0;

	prop = xmlGetProp(cur, (const xmlChar *)key);
	if (prop) {
		ret = atoi((char *)prop);
		xmlFree(prop);
	}
	return ret;

}

static int get_id_index(xmlNodePtr cur)
{
	return get_prop_int(cur, "id") - 1;
}

#define load_cur_content(cur, type, mem) \
(__extension__ \
({ \
		xmlChar *load##mem; \
		load##mem = xmlNodeGetContent(cur); \
		if (load##mem) \
			type->mem = (char *)load##mem; \
}) \
)

#define load_cur_content_with_id(cur, type, mem) \
(__extension__ \
({ \
		xmlChar *load##mem; \
		load##mem = xmlNodeGetContent(cur); \
		if (load##mem) \
			type->mem[get_id_index(cur)] = (char *)load##mem; \
}) \
)

#define load_trigger(cur, event) \
(__extension__ \
({ \
		xmlChar *content = xmlNodeGetContent(cur); \
		event->trigger = get_trigger_by_name((char *)content); \
		xmlFree(content); \
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

int sender_id(struct sender_t *s)
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

struct sender_t *get_sender_by_name(char *name)
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

struct trigger_t *get_trigger_by_name(char *name)
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

struct log_t *get_log_by_name(char *name)
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

static int get_inherit_index(xmlNodePtr cur)
{
	return get_prop_int(cur, "inherit") - 1;
}

#define name_is(cur, key) \
		(xmlStrcmp(cur->name, (const xmlChar *)key) == 0)

static void parse_info(xmlNodePtr cur, struct info_t *info)
{
	int id;
	xmlChar *content;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, info, name);
		else if (name_is(cur, "trigger"))
			load_trigger(cur, info);
		else if (name_is(cur, "channel"))
			load_cur_content(cur, info, channel);
		else if (name_is(cur, "log")) {
			id = get_id_index(cur);
			content = xmlNodeGetContent(cur);
			info->log[id] = get_log_by_name((char *)content);
			xmlFree(content);
		}

		cur = cur->next;
	}
}

static void parse_log(xmlNodePtr cur, struct log_t *log)
{
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, log, name);
		else if (name_is(cur, "type"))
			load_cur_content(cur, log, type);
		else if (name_is(cur, "path"))
			load_cur_content(cur, log, path);
		else if (name_is(cur, "lines"))
			load_cur_content(cur, log, lines);

		cur = cur->next;
	}
}

static void parse_crash(xmlNodePtr cur, struct crash_t *crash)
{

	int id;
	int expression;
	xmlChar *content;

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, crash, name);
		else if (name_is(cur, "trigger"))
			load_trigger(cur, crash);
		else if (name_is(cur, "channel"))
			load_cur_content(cur, crash, channel);
		else if (name_is(cur, "content"))
			load_cur_content_with_id(cur, crash, content);
		else if (name_is(cur, "log")) {
			id = get_id_index(cur);
			content = xmlNodeGetContent(cur);
			crash->log[id] = get_log_by_name((char *)content);
			xmlFree(content);
		} else if (name_is(cur, "data"))
			load_cur_content_with_id(cur, crash, data);
		else if (name_is(cur, "mightcontent")) {
			id = get_id_index(cur);
			expression = get_prop_int(cur, "expression") - 1;
			content = xmlNodeGetContent(cur);
			crash->mightcontent[expression][id] = (char *)content;
		}
		cur = cur->next;
	}


}

static void parse_crashes(xmlNodePtr crashes)
{
	int id;
	xmlNodePtr cur;
	struct crash_t *crash;

	cur = crashes->xmlChildrenNode;

	while (cur) {
		if (is_enable(cur)) {
			crash = malloc(sizeof(*crash));
			id = get_inherit_index(cur);
			if (id >= 0) {
				memcpy(crash, conf.crash[id], sizeof(*crash));
				crash->parents = conf.crash[id];
				crash->level++;
				TAILQ_INSERT_TAIL(&crash->parents->children,
						crash, entries);
			} else {
				memset(crash, 0, sizeof(*crash));
			}
			TAILQ_INIT(&crash->children);
			id = get_id_index(cur);
			conf.crash[id] = crash;
			parse_crash(cur, crash);
		}
		cur = cur->next;
	}
}

static void parse_vm(xmlNodePtr cur, struct vm_t *vm)
{
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, vm, name);
		else if (name_is(cur, "channel"))
			load_cur_content(cur, vm, channel);
		else if (name_is(cur, "interval"))
			load_cur_content(cur, vm, interval);
		else if (name_is(cur, "syncevent"))
			load_cur_content_with_id(cur, vm, syncevent);

		cur = cur->next;
	}
}

static void parse_uptime(xmlNodePtr cur, struct sender_t *sender)
{
	struct uptime_t *uptime;
	int ret;

	uptime = malloc(sizeof(*uptime));
	memset(uptime, 0, sizeof(*uptime));
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, uptime, name);
		else if (name_is(cur, "frequency"))
			load_cur_content(cur, uptime, frequency);
		else if (name_is(cur, "eventhours"))
			load_cur_content(cur, uptime, eventhours);

		cur = cur->next;
	}
	ret = asprintf(&uptime->path, "%s/uptime", sender->outdir);
	if (ret < 0) {
		LOGE("build string failed\n");
		exit(EXIT_FAILURE);
	}
	sender->uptime = uptime;
}

static void parse_trigger(xmlNodePtr cur, struct trigger_t *trigger)
{
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, trigger, name);
		else if (name_is(cur, "type"))
			load_cur_content(cur, trigger, type);
		else if (name_is(cur, "path"))
			load_cur_content(cur, trigger, path);

		cur = cur->next;
	}
}

static void parse_sender(xmlNodePtr cur, struct sender_t *sender)
{
	cur = cur->xmlChildrenNode;
	while (cur) {
		if (name_is(cur, "name"))
			load_cur_content(cur, sender, name);
		else if (name_is(cur, "outdir"))
			load_cur_content(cur, sender, outdir);
		else if (name_is(cur, "maxcrashdirs"))
			load_cur_content(cur, sender, maxcrashdirs);
		else if (name_is(cur, "maxlines"))
			load_cur_content(cur, sender, maxlines);
		else if (name_is(cur, "spacequota"))
			load_cur_content(cur, sender, spacequota);
		else if (name_is(cur, "uptime"))
			parse_uptime(cur, sender);

		cur = cur->next;
	}
}

#define common_parse(node, mem) \
(__extension__ \
({ \
	int id; \
	struct mem##_t *mem; \
\
	node = node->xmlChildrenNode; \
\
	while (node) { \
		if (is_enable(node)) { \
			mem = malloc(sizeof(*mem)); \
			memset(mem, 0, sizeof(*mem)); \
			id = get_id_index(node); \
			conf.mem[id] = mem; \
			parse_##mem(node, mem); \
		} \
		node = node->next; \
	} \
}) \
)

int load_conf(char *path)
{
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
			common_parse(node, sender);
		else if (name_is(node, "triggers"))
			common_parse(node, trigger);
		else if (name_is(node, "vms"))
			common_parse(node, vm);
		else if (name_is(node, "crashes"))
			parse_crashes(node);
		else if (name_is(node, "logs"))
			common_parse(node, log);
		else if (name_is(node, "infos"))
			common_parse(node, info);

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
