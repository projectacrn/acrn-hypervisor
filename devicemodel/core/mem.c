/*-
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

/*
 * Memory ranges are represented with an RB tree. On insertion, the range
 * is checked for overlaps. On lookup, the key has the same base and limit
 * so it can be searched within the range.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "vmm.h"
#include "mem.h"
#include "tree.h"

#define MEMNAMESZ (80)

struct mmio_rb_range {
	RB_ENTRY(mmio_rb_range)	mr_link;	/* RB tree links */
	struct mem_range	mr_param;
	uint64_t                mr_base;
	uint64_t                mr_end;
};

static RB_HEAD(mmio_rb_tree, mmio_rb_range) mmio_rb_root, mmio_rb_fallback;
RB_PROTOTYPE_STATIC(mmio_rb_tree, mmio_rb_range, mr_link, mmio_rb_range_compare);

/*
 * Per-VM cache. Since most accesses from a vCPU will be to
 * consecutive addresses in a range, it makes sense to cache the
 * result of a lookup.
 */
static struct mmio_rb_range	*mmio_hint;

static pthread_rwlock_t mmio_rwlock;

static int
mmio_rb_range_compare(struct mmio_rb_range *a, struct mmio_rb_range *b)
{
	if (a->mr_end < b->mr_base)
		return -1;
	else if (a->mr_base > b->mr_end)
		return 1;
	return 0;
}

static int
mmio_rb_lookup(struct mmio_rb_tree *rbt, uint64_t addr,
		struct mmio_rb_range **entry)
{
	struct mmio_rb_range find, *res;

	find.mr_base = find.mr_end = addr;

	res = RB_FIND(mmio_rb_tree, rbt, &find);

	if (res != NULL) {
		*entry = res;
		return 0;
	}

	return -1;
}

static int
mmio_rb_add(struct mmio_rb_tree *rbt, struct mmio_rb_range *new)
{
	struct mmio_rb_range *overlap;

	overlap = RB_INSERT(mmio_rb_tree, rbt, new);

	if (overlap != NULL) {
#ifdef RB_DEBUG
		printf("overlap detected: new %lx:%lx, tree %lx:%lx\n",
		       new->mr_base, new->mr_end,
		       overlap->mr_base, overlap->mr_end);
#endif

		return -1;
	}

	return 0;
}

#if RB_DEBUG
static void
mmio_rb_dump(struct mmio_rb_tree *rbt)
{
	struct mmio_rb_range *np;

	pthread_rwlock_rdlock(&mmio_rwlock);
	RB_FOREACH(np, mmio_rb_tree, rbt) {
		printf(" %lx:%lx, %s\n", np->mr_base, np->mr_end,
		       np->mr_param.name);
	}
	pthread_rwlock_unlock(&mmio_rwlock);
}
#endif

RB_GENERATE_STATIC(mmio_rb_tree, mmio_rb_range, mr_link, mmio_rb_range_compare);

static int
mem_read(void *ctx, int vcpu, uint64_t gpa, uint64_t *rval, int size, void *arg)
{
	int error;
	struct mem_range *mr = arg;

	error = (*mr->handler)(ctx, vcpu, MEM_F_READ, gpa, size,
			       rval, mr->arg1, mr->arg2);
	return error;
}

static int
mem_write(void *ctx, int vcpu, uint64_t gpa, uint64_t wval, int size, void *arg)
{
	int error;
	struct mem_range *mr = arg;

	error = (*mr->handler)(ctx, vcpu, MEM_F_WRITE, gpa, size,
			       &wval, mr->arg1, mr->arg2);
	return error;
}

int
emulate_mem(struct vmctx *ctx, struct mmio_request *mmio_req)
{
	uint64_t paddr = mmio_req->address;
	int size = mmio_req->size;
	struct mmio_rb_range *entry = NULL;
	int err;

	pthread_rwlock_rdlock(&mmio_rwlock);
	/*
	 * First check the per-VM cache
	 */
	if (mmio_hint && paddr >= mmio_hint->mr_base &&
			paddr <= mmio_hint->mr_end)
		entry = mmio_hint;
	else if (mmio_rb_lookup(&mmio_rb_root, paddr, &entry) == 0)
		/* Update the per-VM cache */
		mmio_hint = entry;
	else if (mmio_rb_lookup(&mmio_rb_fallback, paddr, &entry)) {
		pthread_rwlock_unlock(&mmio_rwlock);
		return -ESRCH;
	}

	pthread_rwlock_unlock(&mmio_rwlock);

	assert(entry != NULL);

	if (mmio_req->direction == REQUEST_READ)
		err = mem_read(ctx, 0, paddr, (uint64_t *)&mmio_req->value,
				size, &entry->mr_param);
	else
		err = mem_write(ctx, 0, paddr, mmio_req->value,
				size, &entry->mr_param);

	return err;
}

static int
register_mem_int(struct mmio_rb_tree *rbt, struct mem_range *memp)
{
	struct mmio_rb_range *entry, *mrp;
	int err;

	err = 0;

	mrp = malloc(sizeof(struct mmio_rb_range));

	if (mrp != NULL) {
		mrp->mr_param = *memp;
		mrp->mr_base = memp->base;
		mrp->mr_end = memp->base + memp->size - 1;
		pthread_rwlock_wrlock(&mmio_rwlock);
		if (mmio_rb_lookup(rbt, memp->base, &entry) != 0)
			err = mmio_rb_add(rbt, mrp);
		pthread_rwlock_unlock(&mmio_rwlock);
		if (err)
			free(mrp);
	} else
		err = -1;

	return err;
}

int
register_mem(struct mem_range *memp)
{
	return register_mem_int(&mmio_rb_root, memp);
}

int
register_mem_fallback(struct mem_range *memp)
{
	return register_mem_int(&mmio_rb_fallback, memp);
}

static int
unregister_mem_int(struct mmio_rb_tree *rbt, struct mem_range *memp)
{
	struct mem_range *mr;
	struct mmio_rb_range *entry = NULL;
	int err;

	pthread_rwlock_wrlock(&mmio_rwlock);
	err = mmio_rb_lookup(rbt, memp->base, &entry);
	if (err == 0) {
		mr = &entry->mr_param;
		if (strncmp(mr->name, memp->name, MEMNAMESZ)) {
			err = -1;
		} else {
			assert(mr->base == memp->base && mr->size == memp->size);
			assert((mr->flags & MEM_F_IMMUTABLE) == 0);
			RB_REMOVE(mmio_rb_tree, rbt, entry);

			/* flush Per-VM cache */
			if (mmio_hint == entry)
				mmio_hint = NULL;
		}
	}
	pthread_rwlock_unlock(&mmio_rwlock);

	if (entry)
		free(entry);

	return err;
}

int
unregister_mem(struct mem_range *memp)
{
	return unregister_mem_int(&mmio_rb_root, memp);
}

int
unregister_mem_fallback(struct mem_range *memp)
{
	return unregister_mem_int(&mmio_rb_fallback, memp);
}

void
init_mem(void)
{
	RB_INIT(&mmio_rb_root);
	RB_INIT(&mmio_rb_fallback);
	pthread_rwlock_init(&mmio_rwlock, NULL);
}
