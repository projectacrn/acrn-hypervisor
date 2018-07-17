/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <fcntl.h>
#include <linux/loop.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <blkid/blkid.h>
#include <ext2fs/ext2fs.h>
#include "log_sys.h"

#define DEV_LOOP_CTL "/dev/loop-control"

static int get_par_startaddr_from_img(const char *img,
					const char *target_parname,
					unsigned long long *start)
{
	blkid_probe pr;
	blkid_partlist ls;
	blkid_partition par;
	int i;
	int nparts;
	const char *par_name;
	unsigned int sector_size;
	unsigned long long par_start;

	if (!img || !target_parname || !start)
		return -1;

	pr = blkid_new_probe_from_filename(img);
	if (!pr) {
		LOGE("blkid new probe failed\n");
		return -1;
	}
	ls = blkid_probe_get_partitions(pr);
	if (!ls) {
		LOGE("blkid get partitions failed\n");
		goto err;
	}
	nparts = blkid_partlist_numof_partitions(ls);
	if (nparts <= 0) {
		LOGE("(%d) partitions in (%s)??\n", nparts, img);
		goto err;
	}

	for (i = 0; i < nparts; i++) {
		par = blkid_partlist_get_partition(ls, i);
		par_name = blkid_partition_get_name(par);
		if (!par_name) {
			LOGW("A partition in (%s) don't have name??\n", img);
			continue;
		}
		if (!strcmp(par_name, target_parname))
			goto found;
	}
	LOGE("no partition of (%s) is named %s\n", img, target_parname);
err:
	blkid_free_probe(pr);
	return -1;
found:
	sector_size = blkid_probe_get_sectorsize(pr);
	par_start = (unsigned long long)blkid_partition_get_start(par);
	*start = par_start * sector_size;
	blkid_free_probe(pr);
	return 0;
}

int loopdev_num_get_free(void)
{
	int loopctlfd;
	int devnr;

	loopctlfd = open(DEV_LOOP_CTL, O_RDONLY);
	if (loopctlfd == -1) {
		LOGE("failed to open %s, error (%s)\n", DEV_LOOP_CTL,
		     strerror(errno));
		return -errno;
	}

	devnr = ioctl(loopctlfd, LOOP_CTL_GET_FREE);
	if (devnr == -1) {
		LOGE("failed to get free loopdev, error (%s)\n",
		     strerror(errno));
		close(loopctlfd);
		return -errno;
	}

	close(loopctlfd);
	return devnr;
}

static int loopdev_set_status(const char *loopdev,
				const struct loop_info64 *info)
{
	int loopfd;
	int res;

	if (!loopdev || !info)
		return -EINVAL;

	loopfd = open(loopdev, O_RDWR);
	if (loopfd == -1) {
		LOGE("failed to open (%s), error(%s)\n", loopdev,
		     strerror(errno));
		return -errno;
	}

	res = ioctl(loopfd, LOOP_SET_STATUS64, info);
	if (res == -1) {
		LOGE("failed to set info to (%s), error(%s)\n", loopdev,
		     strerror(errno));
		close(loopfd);
		return -errno;
	}

	close(loopfd);
	return 0;
}

static int loopdev_set_img(const char *loopdev, const char *img_path)
{
	int loopfd;
	int imgfd;
	int res;

	if (!loopdev || !img_path)
		return -EINVAL;

	loopfd = open(loopdev, O_WRONLY);
	if (loopfd == -1) {
		LOGE("failed to open %s, error (%s)\n", loopdev,
		     strerror(errno));
		return -errno;
	}

	imgfd = open(img_path, O_RDONLY);
	if (imgfd == -1) {
		LOGE("failed to open %s, error (%s)\n", img_path,
		     strerror(errno));
		close(loopfd);
		return -errno;
	}

	res = ioctl(loopfd, LOOP_SET_FD, imgfd);
	if (res == -1) {
		LOGE("failed to set (%s) to (%s), error (%s)\n", img_path,
		     loopdev, strerror(errno));
		close(loopfd);
		close(imgfd);
		return -errno;
	}

	close(loopfd);
	close(imgfd);
	return 0;
}

int loopdev_set_img_par(const char *loopdev, const char *img_path,
			const char *parname)
{
	struct loop_info64 info;
	unsigned long long par_start;
	int res;

	if (!loopdev || !img_path || !parname)
		return -1;

	res = get_par_startaddr_from_img(img_path, parname, &par_start);
	if (res == -1) {
		LOGE("failed to get data par startaddr\n");
		return -1;
	}

	res = loopdev_set_img(loopdev, img_path);
	if (res) {
		LOGE("failed to set img (%s) to (%s), error (%s)\n",
		       img_path, loopdev, strerror(-res));
		return -1;
	}

	memset(&info, 0, sizeof(info));
	info.lo_offset = par_start;

	res = loopdev_set_status(loopdev, &info);
	if (res < 0) {
		LOGE("failed to set loopdev, error (%s)\n", strerror(-res));
		return -1;
	}
	return 0;
}

int loopdev_check_parname(const char *loopdev, const char *parname)
{
	struct ext2_super_block super;
	int fd;
	const int skiprate = 512;
	loff_t sk = 0;

	if (!loopdev || !parname)
		return -ENOENT;

	/* quickly find super block */
	fd = open(loopdev, O_RDONLY);
	if (fd == -1) {
		LOGE("failed to open (%s), error(%s)\n", loopdev,
		     strerror(errno));
		return -errno;
	}
	for (; lseek64(fd, sk, SEEK_SET) != -1 &&
	       read(fd, &super, 512) == 512; sk += skiprate) {
		if (super.s_magic != EXT2_SUPER_MAGIC)
			continue;

		LOGD("find super block at +%ld\n", sk);
		/* only look into the primary super block */
		if (super.s_volume_name[0]) {
			close(fd);
			return !strcmp(super.s_volume_name, parname);
		}
		break;
	}

	close(fd);
	return 0;
}
