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

#include <openssl/sha.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "property.h"
#include "log_sys.h"
#include "fsutils.h"

#define MACHINE_ID		"/etc/machine-id"
#define OS_VERSION		"/usr/lib/os-release"
#define OS_VERSION_KEY		"VERSION_ID="
#define DEVICE_ID_UNKNOWN	"UnknownId"
#define LOG_UUID		"uuid.txt"
#define LOG_BUILDID		"buildid.txt"

static void get_device_id(struct sender_t *sender)
{
	int ret;
	char *loguuid;


	ret = asprintf(&loguuid, "%s/%s", sender->outdir, LOG_UUID);
	if (ret < 0) {
		LOGE("compute string failed, out of memory\n");
		return;
	}

	ret = file_read_string(MACHINE_ID, guuid, BUILD_VERSION_SIZE);
	if (ret <= 0)
		LOGE("Could not get mmc id: %d (%s)\n",
		     ret, strerror(-ret));
	else
		goto write;

	LOGE("Could not find DeviceId, set it to '%s'\n",
	     DEVICE_ID_UNKNOWN);
	strncpy(guuid, DEVICE_ID_UNKNOWN, UUID_SIZE);
	guuid[UUID_SIZE - 1] = '\0';

write:
	overwrite_file(loguuid, guuid);
	free(loguuid);
}

static int get_buildversion(struct sender_t *sender)
{
	int ret;
	char lastbuild[BUILD_VERSION_SIZE];
	char *logbuildid;
	char *currentbuild = gbuildversion;

	ret = file_read_key_value(gbuildversion, sizeof(gbuildversion),
				  OS_VERSION, OS_VERSION_KEY,
				  strlen(OS_VERSION_KEY));
	if (ret <= 0) {
		LOGE("failed to get version from %s, error (%s)\n",
		     OS_VERSION, strerror(-ret));
		return ret;
	}

	ret = asprintf(&logbuildid, "%s/%s", sender->outdir, LOG_BUILDID);
	if (ret < 0) {
		LOGE("compute string failed, out of memory\n");
		return ret;
	}

	ret = file_read_string(logbuildid, lastbuild, BUILD_VERSION_SIZE);
	if (ret == -ENOENT ||
	    !ret ||
	    (ret > 0 && strcmp(currentbuild, lastbuild))) {
		/* build changed or file not found, overwrite it */
		ret = overwrite_file(logbuildid, gbuildversion);
		if (ret) {
			LOGE("create (%s) failed, error (%s)\n", logbuildid,
			     strerror(-ret));
			goto free;
		}

		sender->sw_updated = 1;
		ret = 0;
	} else if (ret < 0) {
		LOGE("Cannot read %s, error (%s)\n",
		     logbuildid, strerror(errno));
	} else {
		/* buildid is the same */
		sender->sw_updated = 0;
		ret = 0;
	}
free:
	free(logbuildid);
	return ret;
}

int swupdated(struct sender_t *sender)
{
	return sender->sw_updated;
}

int init_properties(struct sender_t *sender)
{
	int ret;

	ret = get_buildversion(sender);
	if (ret) {
		LOGE("init properties failed\n");
		return ret;
	}
	get_device_id(sender);
	return 0;
}
