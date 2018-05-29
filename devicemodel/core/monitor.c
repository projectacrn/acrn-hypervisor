/*
 * Project Acrn
 * Acrn-dm-monitor
 *
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author: TaoYuhong <yuhong.tao@intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <pthread.h>
#include "dm.h"
#include "monitor.h"
#include "acrn_mngr.h"

/* helpers */
/* Check if @path is a directory, and create if not exist */
static int check_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st)) {
		if (mkdir(path, 0666)) {
			perror(path);
			return -1;
		}
		return 0;
	}

	if (S_ISDIR(st.st_mode))
		return 0;

	fprintf(stderr, "%s exist, and not a directory!\n", path);
	return -1;
}

static int monitor_fd = -1;

int monitor_init(struct vmctx *ctx)
{
	int ret;
	char path[128] = {};

	ret = check_dir("/run/acrn/");
	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto dir_err;
	}

	ret = check_dir("/run/acrn/mngr");
	if (ret) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto dir_err;
	}

	snprintf(path, sizeof(path) - 1, "%s.monitor", vmname);

	monitor_fd = mngr_open_un(path, MNGR_SERVER);
	if (monitor_fd < 0) {
		fprintf(stderr, "%s %d\r\n", __FUNCTION__, __LINE__);
		goto server_err;
	}

	return 0;

 server_err:
 dir_err:
	return -1;
}

void monitor_close(void)
{
	if (monitor_fd >= 0)
		mngr_close(monitor_fd);
}
