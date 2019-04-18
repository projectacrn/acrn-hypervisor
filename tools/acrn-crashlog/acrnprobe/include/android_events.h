/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __ANDROID_EVENTS_H__
#define __ANDROID_EVENTS_H__
#include "load_conf.h"

extern char *loop_dev;

#define VMEVT_HANDLED 0
#define VMEVT_DEFER -1
#define VMEVT_MISSLOG -2

#define ANDROID_LOGS_DIR "/logs/"
#define IGN_SPACES "%*[[[:space:]]*]"
#define IGN_RESTS "%*[[.]*]"
#define IGN_ONEWORD "%*[[^[:space:]]*]" IGN_SPACES
#define VM_NAME_FMT "%[[A-Z0-9]{3}]" IGN_SPACES

/* These below macros were defined to obtain strings from
 * andorid history_event
 */
#define ANDROID_WORD_LEN 32

/* Strings are constructed by A-Z, len < 8, e.g., CRASH REBOOT */
#define ANDROID_ENEVT_FMT "%[[A-Z]{1,7}]" IGN_SPACES
/* Hashkeys are constructed by 0-9&a-z, len = 20, e.g., 0b34ae1afba54aee5cd0. */
#define ANDROID_KEY_FMT "%[[0-9a-z]{20}]" IGN_SPACES
/* Strings, e.g., 2017-11-11/03:12:59 */
#define ANDROID_LONGTIME_FMT "%[[0-9:/-]{15,20}]" IGN_SPACES
/* It's a time or a subtype of event, e.g., JAVACRASH POWER-ON 424874:19:56 */
#define ANDROID_TYPE_FMT "%[[A-Z0-9_:-]{3,16}]" IGN_SPACES
#define ANDROID_LINE_REST_FMT "%[[^\n]*]" IGN_RESTS

void refresh_vm_history(struct sender_t *sender,
			int (*fn)(const char*, size_t, const struct vm_t *));

#endif
