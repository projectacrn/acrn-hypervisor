/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
/*
 * MEI device virtualization.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

#include <linux/uuid.h>
#include <linux/mei.h>

#include "types.h"
#include "vmmapi.h"
#include "mevent.h"
#include "pci_core.h"
#include "virtio.h"
#include "dm.h"

#include "mei.h"

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define DEV_NAME_SIZE sizeof(((struct dirent *)0)->d_name)

#ifndef UUID_STR_LEN
#define UUID_STR_LEN 37
#endif

#ifndef GUID_INIT
#define GUID_INIT(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
	UUID_LE(a, b, c, d0, d1, d2, d3, d4, d5, d6, d7)
#endif

static int guid_parse(const char *str, size_t maxlen, guid_t *guid)
{
	const char *p = "00000000-0000-0000-0000-000000000000";
	const size_t len = strnlen(p, UUID_STR_LEN);
	uint32_t a;
	uint16_t b, c;
	uint8_t d[2], e[6];
	char buf[3];
	unsigned int i;

	if (strnlen(str, maxlen) != len)
		return -1;

	for (i = 0; i < len; i++) {
		if (str[i] == '-') {
			if (p[i] == '-')
				continue;
			else
				return -1;
		} else if (!isxdigit(str[i])) {
			return -1;
		}
	}

	a = strtoul(str +  0, NULL, 16);
	b = strtoul(str +  9, NULL, 16);
	c = strtoul(str + 14, NULL, 16);

	buf[2] = 0;
	for (i = 0; i < 2; i++) {
		buf[0] = str[19 + i * 2];
		buf[1] = str[19 + i * 2 + 1];
		d[i] = strtoul(buf, NULL, 16);
	}

	for (i = 0; i < 6; i++) {
		buf[0] = str[24 + i * 2];
		buf[1] = str[24 + i * 2 + 1];
		e[i] = strtoul(buf, NULL, 16);
	}

	*guid = GUID_INIT(a, b, c,
			  d[0], d[1], e[0], e[1], e[2], e[3], e[4], e[5]);

	return 0;
}

static int guid_unparse(const guid_t *guid, char *str, size_t len)
{
	unsigned int i;
	size_t pos = 0;

	if (len < UUID_STR_LEN)
		return -EINVAL;

	pos += snprintf(str + pos, len - pos, "%02x", guid->b[3]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[2]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[1]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[0]);
	str[pos] = '-';
	pos++;
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[5]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[4]);
	str[pos] = '-';
	pos++;
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[7]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[6]);
	str[pos] = '-';
	pos++;
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[8]);
	pos += snprintf(str + pos, len - pos, "%02x", guid->b[9]);
	str[pos] = '-';
	pos++;
	for (i = 10; i < 16; i++)
		pos += snprintf(str + pos, len - pos, "%02x", guid->b[i]);

	return 0;
}

struct refcnt {
	void (*destroy)(const struct refcnt *ref);
	int count;
};

static inline void
refcnt_get(const struct refcnt *ref)
{
	__sync_add_and_fetch((int *)&ref->count, 1);
}

static inline void
refcnt_put(const struct refcnt *ref)
{
	if (__sync_sub_and_fetch((int *)&ref->count, 1) == 0)
		ref->destroy(ref);
}

static int mei_sysfs_read_property_file(const char *fname, char *buf, size_t sz)
{
	int fd;
	int rc;

	if (!buf)
		return -EINVAL;

	if (!sz)
		return 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		DPRINTF("open failed %s %d\n", fname, errno);
		return -1;
	}

	rc = read(fd, buf, sz);

	close(fd);

	return rc;
}

static int mei_sysfs_read_property_u8(const char *fname, uint8_t *u8_property)
{
	char buf[4] = {0};
	unsigned long int res;

	if (mei_sysfs_read_property_file(fname, buf, sizeof(buf) - 1) < 0)
		return -1;

	res = strtoul(buf, NULL, 10);
	if (res >= 256)
		return -1;

	*u8_property = (uint8_t)res;

	return 0;
}

static int mei_sysfs_read_property_u32(const char *fname,
				       uint32_t *u32_property)
{
	char buf[32] = {0};
	unsigned long int res;

	if (mei_sysfs_read_property_file(fname, buf, sizeof(buf) - 1) < 0)
		return -1;

	res = strtoul(buf, NULL, 10);
	if (res == ULONG_MAX)
		return -1;

	*u32_property = res;

	return 0;
}

static int mei_sysfs_read_property_uuid(char *fname, guid_t *uuid)
{
	char buf[UUID_STR_LEN] = {0};

	if (mei_sysfs_read_property_file(fname, buf, sizeof(buf) - 1) < 0)
		return -1;

	return guid_parse(buf, sizeof(buf), uuid);
}
