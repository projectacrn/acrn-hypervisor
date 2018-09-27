/*
 * Copyright (C) <2018> Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LOG_SYS_H__
#define __LOG_SYS_H__

#include <syslog.h>
#include <systemd/sd-journal.h>

void debug_log(int level, const char *func, int line, ...);

#define LOG_LEVEL LOG_WARNING

#ifdef DEBUG_ACRN_CRASHLOG
#define LOGE(...) \
		debug_log(LOG_ERR, __func__, __LINE__,  __VA_ARGS__)

#define LOGW(...) \
		debug_log(LOG_WARNING, __func__, __LINE__, __VA_ARGS__)

#define LOGI(...) \
		debug_log(LOG_INFO, __func__, __LINE__, __VA_ARGS__)

#define LOGD(...) \
		debug_log(LOG_DEBUG, __func__, __LINE__, __VA_ARGS__)
#else
#define ac_log(level, ...) \
	do { \
		if (level <= LOG_LEVEL) \
			sd_journal_print(level, __VA_ARGS__); \
	} while (0)

#define LOGE(...) \
		ac_log(LOG_ERR, __VA_ARGS__)

#define LOGW(...) \
		ac_log(LOG_WARNING, __VA_ARGS__)

#define LOGI(...) \
		ac_log(LOG_INFO, __VA_ARGS__)

#define LOGD(...) \
		ac_log(LOG_DEBUG, __VA_ARGS__)
#endif

#endif
