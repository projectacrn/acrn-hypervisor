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

#define IGN_SPACES "%*[ ]"
#define IGN_RESTS "%*c"
#define IGN_ONEWORD "%*[^ ]" IGN_SPACES
#define VM_NAME_FMT "%8[A-Z0-9]" IGN_SPACES

/* These below macros were defined to obtain strings from
 * andorid history_event
 */
#define ANDROID_WORD_LEN 32

/* Strings are constructed by A-Z, len < 8, e.g., CRASH REBOOT */
#define ANDROID_ENEVT_FMT "%8[A-Z]" IGN_SPACES
/* Hashkeys are constructed by 0-9&a-z, len = 20, e.g., 0b34ae1afba54aee5cd0.
 * But the hashkey was printed to history_event file in andorid side by using
 * format "%22s", so also using %22 here.
 */
#define ANDROID_KEY_FMT "%22[0-9a-z]" IGN_SPACES
/* Strings, e.g., 2017-11-11/03:12:59 */
#define ANDROID_LONGTIME_FMT "%20[0-9:/-]" IGN_SPACES
/* It's a time or a subtype of event, e.g., JAVACRASH POWER-ON 424874:19:56 */
#define ANDROID_TYPE_FMT "%16[A-Z0-9_:-]" IGN_SPACES
#define ANDROID_LINE_REST_FMT "%4096[^\n]" IGN_RESTS

void refresh_vm_history(const struct sender_t *sender,
			int (*fn)(const char*, const struct vm_t *));

#endif
