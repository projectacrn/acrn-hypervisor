/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __VMRECORD_H__
#define __VMRECORD_H__
#include "pthread.h"

#define VMRECORD_HEAD_LINES 10
#define VMRECORD_TAG_LEN 9
#define VMRECORD_TAG_WAITING_SYNC	"      <=="
#define VMRECORD_TAG_NOT_FOUND		"NOT_FOUND"
#define VMRECORD_TAG_MISS_LOG		"MISS_LOGS"
#define VMRECORD_TAG_ON_GOING		" ON_GOING"
#define VMRECORD_TAG_NO_RESOURCE	"NO_RESORC"
#define VMRECORD_TAG_SUCCESS		"         "

enum vmrecord_mark_t {
	SUCCESS,
	NOT_FOUND,
	WAITING_SYNC,
	ON_GOING,
	NO_RESRC,
	MISS_LOG
};

struct vmrecord_t {
	char		*path;
	pthread_mutex_t	mtx;
	struct mm_file_t *recos;
};

int vmrecord_last(struct vmrecord_t *vmrecord, const char *vm_name,
		size_t nlen, char *vmkey, size_t ksize);
int vmrecord_mark(struct vmrecord_t *vmrecord, const char *vmkey,
		   size_t klen, enum vmrecord_mark_t type);
int vmrecord_open_mark(struct vmrecord_t *vmrecord, const char *vmkey,
		   size_t klen, enum vmrecord_mark_t type);
int vmrecord_gen_ifnot_exists(struct vmrecord_t *vmrecord);
int vmrecord_new(struct vmrecord_t *vmrecord, const char *vm_name,
		  const char *key);


#endif
