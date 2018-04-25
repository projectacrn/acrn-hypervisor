/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LOG_SYS_H__
#define __LOG_SYS_H__

#include <stdarg.h>
#include <syslog.h>

void do_log(int level,
#ifdef DEBUG_ACRN_CRASHLOG
		const char *func, int line,
#endif
		...);

#define MAX_LOG_LEN 1024
#define LOG_LEVEL LOG_WARNING

#ifdef DEBUG_ACRN_CRASHLOG
#define LOGE(...) \
		do_log(LOG_ERR, __func__, __LINE__,  __VA_ARGS__)

#define LOGW(...) \
		do_log(LOG_WARNING, __func__, __LINE__, __VA_ARGS__)

#define LOGI(...) \
		do_log(LOG_INFO, __func__, __LINE__, __VA_ARGS__)

#define LOGD(...) \
		do_log(LOG_DEBUG, __func__, __LINE__, __VA_ARGS__)
#else
#define LOGE(...) \
		do_log(LOG_ERR, __VA_ARGS__)

#define LOGW(...) \
		do_log(LOG_WARNING, __VA_ARGS__)

#define LOGI(...) \
		do_log(LOG_INFO, __VA_ARGS__)

#define LOGD(...) \
		do_log(LOG_DEBUG, __VA_ARGS__)
#endif

#endif
