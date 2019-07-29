/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "sbuf.h"

#define PCPU_NUM        	4
#define TRACE_ELEMENT_SIZE      32	/* byte */
#define TRACE_ELEMENT_NUM	((4 * 1024 * 1024 - 64) / TRACE_ELEMENT_SIZE)
#define PAGE_SIZE		4096
#define PAGE_MASK		(~(PAGE_SIZE - 1))
#define MMAP_SIZE		(4 * 1024 * 1024)
/*
#define MMAP_SIZE 		((TRACE_ELEMENT_SIZE * TRACE_ELEMENT_NUM \
				+ PAGE_SIZE - 1) & PAGE_MASK)
*/
#define TRACE_FILE_NAME_LEN	32
#define TRACE_FILE_DIR_LEN	(TRACE_FILE_NAME_LEN - 3)
#define TRACE_FILE_ROOT		"acrntrace/"
#define DEV_PATH_LEN		18
#define TIME_STR_LEN		16
#define CMD_MAX_LEN		48

#define pr_fmt(fmt)             "acrntrace: " fmt
#define pr_info(fmt, ...)       printf(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...)        printf(pr_fmt(fmt), ##__VA_ARGS__)

#ifdef DEBUG
#define pr_dbg(fmt, ...)        printf(pr_fmt(fmt), ##__VA_ARGS__)
#else
#define pr_dbg(fmt, ...)
#endif

/*
 * flags:
 * FLAG_TO_REL   - resources need to be release
 * FLAG_CLEAR_BUF - to clear buffered old data
 */
#define FLAG_TO_REL		(1UL << 0)
#define FLAG_CLEAR_BUF		(1UL << 1)

#define foreach_dev(dev_id)                                       \
        for ((dev_id) = 0; (dev_id) < (dev_cnt); (dev_id)++)

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

typedef struct {
	uint64_t tsc;
	uint64_t id;
	union {
		struct {
			uint32_t a, b, c, d;
		};
		struct {
			uint8_t a1, a2, a3, a4;
			uint8_t b1, b2, b3, b4;
			uint8_t c1, c2, c3, c4;
			uint8_t d1, d2, d3, d4;
		};
		struct {
			uint64_t e;
			uint64_t f;
		};
		char str[16];
	};
} trace_ev_t;

typedef struct {
	uint32_t devid;
	int exit_flag;
	int trace_fd;
	shared_buf_t *sbuf;
	pthread_mutex_t *sbuf_lock;
} param_t;

typedef struct {
	int dev_fd;
	char dev_name[DEV_PATH_LEN];
	pthread_t thrd;
	param_t param;
} reader_struct;
