/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include "fsutils.h"
#include "startupreason.h"
#include "log_sys.h"

#define CURRENT_KERNEL_CMDLINE "/proc/cmdline"

static int get_cmdline_bootreason(char *bootreason, const size_t limit)
{
	int res;
	unsigned long size;
	char *start, *p1, *p2, *end;
	char *cmdline;
	const char *key = "ABL.reset=";

	res = read_file(CURRENT_KERNEL_CMDLINE, &size, (void *)&cmdline);
	if (res < 0) {
		LOGE("failed to read file %s - %s\n",
		     CURRENT_KERNEL_CMDLINE, strerror(errno));
		return -1;
	}
	if (!size) {
		LOGW("empty file (%s)\n", CURRENT_KERNEL_CMDLINE);
		return 0;
	}

	start = strstr(cmdline, key);
	if (!start) {
		LOGW("can't find reboot reason with key (%s) in cmdline\n",
		     key);
		free(cmdline);
		return 0;
	}

	/* if the string contains ' ' or '\n', break it by '\0' */
	start += strlen(key);
	p1 = strchr(start, ' ');
	p2 = strchr(start, '\n');
	if (p2 && p1)
		end = MIN(p1, p2);
	else
		end = MAX(p1, p2);

	if (!end)
		end = cmdline + size;

	const size_t len = MIN((size_t)(end - start), (size_t)(limit - 1));

	if (len > 0) {
		memcpy(bootreason, start, len);
		*(bootreason + len) = 0;
	}

	free(cmdline);
	return len;
}

static int get_default_bootreason(char *bootreason, const size_t limit)
{
	int len;
	int i;

	len = get_cmdline_bootreason(bootreason, limit);
	if (len <= 0)
		return len;

	for (i = 0; i < len; i++)
		bootreason[i] = toupper(bootreason[i]);

	return len;

}

void read_startupreason(char *startupreason, const size_t limit)
{
	int res;
	static char reboot_reason_cache[REBOOT_REASON_SIZE];

	if (!startupreason || !limit)
		return;

	if (!reboot_reason_cache[0]) {
		/* fill cache */
		res = get_default_bootreason(reboot_reason_cache,
					     sizeof(reboot_reason_cache));
		if (res <= 0)
			strncpy(reboot_reason_cache, "UNKNOWN",
				sizeof(reboot_reason_cache));
	}

	const size_t len = MIN(strnlen(reboot_reason_cache, REBOOT_REASON_SIZE),
			       limit - 1);

	memcpy(startupreason, reboot_reason_cache, len);
	*(startupreason + len) = 0;
	return;
}
