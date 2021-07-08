/*-
 * Copyright (C) 2005-2011 HighPoint Technologies, Inc.
 * Copyright (c) 2017 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef LIST_H_
#define LIST_H_

#include <types.h>

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define INIT_LIST_HEAD(ptr) do { (ptr)->next = (ptr); (ptr)->prev = (ptr); } \
	while (0)

static inline void list_add_node(struct list_head *new_node, struct list_head *prev,
	struct list_head *next)
{
	next->prev = new_node;
	new_node->next = next;
	new_node->prev = prev;
	prev->next = new_node;
}

static inline void list_add(struct list_head *new_node, struct list_head *head)
{
	list_add_node(new_node, head, head->next);
}

static inline void list_add_tail(struct list_head *new_node,
	struct list_head *head)
{
	list_add_node(new_node, head->prev, head);
}

static inline void list_del_node(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(const struct list_head *entry)
{
	list_del_node(entry->prev, entry->next);
}

static inline void list_del_init(struct list_head *entry)
{
	list_del_node(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

static inline _Bool list_empty(const struct list_head *head)
{
	return head->next == head;
}

static inline void list_splice_node(const struct list_head *list,
				 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

static inline void list_splice(const struct list_head *list, struct list_head *head)
{
	if (!list_empty(list)) {
		list_splice_node(list, head);
	}
}

static inline void list_splice_init(struct list_head *list,
	struct list_head *head)
{
	if (!list_empty(list)) {
		list_splice_node(list, head);
		INIT_LIST_HEAD(list);
	}
}

#define container_of(ptr, type, member) \
	((type *)(((char *)(ptr)) - offsetof(type, member)))

#define list_for_each(pos, head) \
	for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define list_for_each_safe(pos, n, head) \
	for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); \
		(pos) = (n), (n) = (pos)->next)

#define get_first_item(attached, type, member) \
	((type *)((char *)((attached)->next)-(uint64_t)(&((type *)0)->member)))

static inline void
hlist_del(struct hlist_node *n)
{

        if (n->next != NULL) {
                n->next->pprev = n->pprev;
	}
        *n->pprev = n->next;
}

static inline void
hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{

	n->next = h->first;
	if (h->first != NULL) {
		h->first->pprev = &n->next;
	}
	h->first = n;
	n->pprev = &h->first;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
	for (pos = (head)->first; (pos) != NULL; pos = (pos)->next)

#endif /* LIST_H_ */
