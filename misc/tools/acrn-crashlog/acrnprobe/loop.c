/*
 * Copyright (C) 2018 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <linux/loop.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <utime.h>
#include <string.h>
#include <blkid/blkid.h>
#include <ext2fs/ext2fs.h>
#include "fsutils.h"
#include "log_sys.h"

#define DEV_LOOP_CTL "/dev/loop-control"

struct walking_inode_data {
	const char *current_out_native_dirpath;
	int dumped_count;
};

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

/**
 * Align the file's perms, only print WARNING if errors occurred in this
 * function.
 *
 * Note: Drop user and group.
 */
static void align_props(int fd, const char *name,
			const struct ext2_inode *inode)
{
	int res;
	struct utimbuf ut;

	if (!inode || !name)
		return;

	if (fd >= 0)
		res = fchmod(fd, inode->i_mode);
	else
		res = chmod(name, inode->i_mode);

	if (res == -1)
		LOGW("failed to exec (xchmod), error (%s)\n", strerror(errno));

	ut.actime = inode->i_atime;
	ut.modtime = inode->i_mtime;
	res = utime(name, &ut);
	if (res == -1)
		LOGW("failed to exec (utime), error (%s)\n", strerror(errno));
}

static int e2fs_get_inodenum_by_fpath(ext2_filsys fs, const char *fpath,
					ext2_ino_t *out_ino)
{
	ext2_ino_t root;
	ext2_ino_t cwd;
	errcode_t res;

	if (!fs || !fpath || !out_ino)
		return -1;

	root = EXT2_ROOT_INO;
	cwd = EXT2_ROOT_INO;

	res = ext2fs_namei(fs, root, cwd, fpath, out_ino);
	if (res) {
		LOGE("ext2fs failed to get ino, path (%s), error (%s)\n",
		       fpath, error_message(res));
		return -1;
	}

	return 0;
}

static int e2fs_read_inode_by_inodenum(ext2_filsys fs, ext2_ino_t ino,
					struct ext2_inode *inode)
{
	errcode_t res;

	if (!fs || !ino || !inode)
		return -1;

	res = ext2fs_read_inode(fs, ino, inode);
	if (res) {
		LOGE("ext2fs failed to get inode, ino (%d), error (%s)\n",
		     ino, error_message(res));
		return -1;
	}

	return 0;
}

static int e2fs_dump_file_by_inodenum(ext2_filsys fs, ext2_ino_t ino,
					const char *out_fp)
{
	errcode_t res;
	int ret = 0;
	int fd;
	unsigned int got;
	struct ext2_inode inode;
	ext2_file_t e2_file;
	char *buf;
	ssize_t write_b;

	if (!fs || !ino || !out_fp)
		return -1;

	res = e2fs_read_inode_by_inodenum(fs, ino, &inode);
	if (res) {
		LOGE("ext2fs failed to read inode, error (%s)\n",
		     error_message(res));
		return -1;
	}

	fd = open(out_fp, O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE, 0666);
	if (fd == -1) {
		LOGE("open (%s) failed, error (%s)\n", out_fp, strerror(errno));
		return -1;
	}

	/* open with read only */
	res = ext2fs_file_open2(fs, ino, &inode, 0, &e2_file);
	if (res) {
		LOGE("ext2fs failed to open file, ino (%d), error (%s)\n",
		       ino, error_message(res));
		close(fd);
		return -1;
	}

	res = ext2fs_get_mem(fs->blocksize, &buf);
	if (res) {
		LOGE("ext2fs failed to get mem, error (%s)\n",
		     error_message(res));
		close(fd);
		ext2fs_file_close(e2_file);
		return -1;
	}

	while (1) {
		res = ext2fs_file_read(e2_file, buf, fs->blocksize, &got);
		/* got equals zero in failed case */
		if (res) {
			LOGE("ext2fs failed to read (%u), error (%s)\n",
			     ino, error_message(res));
			ret = -1;
		}
		if (!got)
			break;

		write_b = write(fd, buf, got);
		if ((unsigned int)write_b != got) {
			LOGE("failed to write file (%s), error (%s)\n",
			     out_fp, strerror(errno));
			ret = -1;
			break;
		}
	}
	align_props(fd, out_fp, &inode);
	if (buf)
		ext2fs_free_mem(&buf);
	/* ext2fs_file_close only failed in flush process */
	ext2fs_file_close(e2_file);
	close(fd);

	return ret;
}

int e2fs_dump_file_by_fpath(ext2_filsys fs, const char *in_fp,
			const char *out_fp)
{
	int res;
	ext2_ino_t ino;

	if (!fs || !in_fp || !out_fp)
		return -1;

	res = e2fs_get_inodenum_by_fpath(fs, in_fp, &ino);
	if (res)
		return res;

	return e2fs_dump_file_by_inodenum(fs, ino, out_fp);
}

static int e2fs_read_file_by_inodenum(ext2_filsys fs, ext2_ino_t ino,
					void **out_data, unsigned long *size)
{
	errcode_t res;
	unsigned int got;
	struct ext2_inode inode;
	ext2_file_t e2_file;
	__u64 _size;
	char *buf;

	if (!fs || !ino || !out_data || !size)
		return -1;

	res = e2fs_read_inode_by_inodenum(fs, ino, &inode);
	if (res) {
		LOGE("ext2fs failed to read inode, error (%s)\n",
		     error_message(res));
		return -1;
	}

	_size = EXT2_I_SIZE(&inode);
	if (!_size) {
		LOGW("try to read a empty file\n");
		*size = 0;
		*out_data = 0;
		return 0;
	}

	/* open with read only */
	res = ext2fs_file_open2(fs, ino, &inode, 0, &e2_file);
	if (res) {
		LOGE("ext2fs failed to open file, ino (%d), error (%s)\n",
		       ino, error_message(res));
		return -1;
	}

	res = ext2fs_get_mem(_size + 1, &buf);
	if (res) {
		LOGE("ext2fs failed to get mem, error (%s)\n",
		     error_message(res));
		ext2fs_file_close(e2_file);
		return -1;
	}

	res = ext2fs_file_read(e2_file, buf, _size, &got);
	/* got equals zero in failed case */
	if (res) {
		LOGE("ext2fs failed to read (%u), error (%s)\n",
		     ino, error_message(res));
		goto err;
	}

	/* ext2fs_file_close only failed in flush process */
	ext2fs_file_close(e2_file);

	*size = _size;
	buf[_size] = 0;
	*out_data = buf;

	return 0;
err:
	free(buf);
	ext2fs_file_close(e2_file);
	return -1;
}

int e2fs_read_file_by_fpath(ext2_filsys fs, const char *in_fp,
			 void **out_data, unsigned long *size)
{
	int res;
	ext2_ino_t ino;

	if (!fs || !in_fp || !out_data || !size)
		return -1;

	res = e2fs_get_inodenum_by_fpath(fs, in_fp, &ino);
	if (res)
		return res;

	return e2fs_read_file_by_inodenum(fs, ino, out_data, size);
}

static int dump_inode_recursively_by_inodenum(ext2_filsys fs, ext2_ino_t ino,
						struct walking_inode_data *data,
						const char *fname);
static int callback_for_subentries(struct ext2_dir_entry *dirent,
				int offset EXT2FS_ATTR((unused)),
				int blocksize EXT2FS_ATTR((unused)),
				char *buf EXT2FS_ATTR((unused)),
				void *private)
{
	char fname[EXT2_NAME_LEN + 1];
	struct walking_inode_data *data = private;
	int len;

	len = dirent->name_len & 0xFF; /* EXT2_NAME_LEN = 255 */
	strncpy(fname, dirent->name, len);
	fname[len] = 0;

	return dump_inode_recursively_by_inodenum(NULL, dirent->inode,
						  data, fname);
}

static int dump_inode_recursively_by_inodenum(ext2_filsys fs, ext2_ino_t ino,
						struct walking_inode_data *data,
						const char *fname)
{
	int res;
	char *out_fpath;
	errcode_t err;
	static ext2_filsys fs_for_dump;
	struct ext2_inode inode;

	if (!ino || !data || !data->current_out_native_dirpath || !fname)
		goto abort;

	/* caller is not callback_for_subentries */
	if (fs)
		fs_for_dump = fs;

	if (!strcmp(fname, ".") || !strcmp(fname, ".."))
		return 0;

	res = asprintf(&out_fpath, "%s/%s", data->current_out_native_dirpath,
		       fname);
	if (res == -1) {
		LOGE("failed to construct target file name, ");
		goto abort;
	}

	res = e2fs_read_inode_by_inodenum(fs_for_dump, ino, &inode);
	if (res) {
		LOGE("ext2fs failed to read inode, ");
		goto abort_free;
	}

	if (LINUX_S_ISREG(inode.i_mode)) {
		/* do dump for file */
		res = e2fs_dump_file_by_inodenum(fs_for_dump, ino, out_fpath);
		if (res) {
			LOGE("ext2fs failed to dump file, ");
			goto abort_free;
		}
		data->dumped_count++;
	} else if (LINUX_S_ISDIR(inode.i_mode)) {
		/* mkdir for directory and dump the subentry */
		res = mkdir(out_fpath, 0700);
		if (res == -1 && errno != EEXIST) {
			LOGE("failed to mkdir (%s), error (%s), ", out_fpath,
			     strerror(errno));
			goto abort_free;
		}
		data->dumped_count++;

		data->current_out_native_dirpath = out_fpath;
		err = ext2fs_dir_iterate(fs_for_dump, ino, 0, 0,
					 callback_for_subentries,
					 (void *)data);
		if (err) {
			LOGE("ext2fs failed to iterate dir, errno (%s), ",
			     error_message(err));
			goto abort_free;
		}
		align_props(-1, out_fpath, &inode);
	}
	/* else ignore the rest types, such as link, socket, fifo, ... */

	free(out_fpath);
	return 0;

abort_free:
	free(out_fpath);
abort:
	LOGE("dump dir aborted...\n");
	return DIRENT_ABORT;
}

int e2fs_dump_dir_by_dpath(ext2_filsys fs, const char *in_dp,
			const char *out_dp, int *count)
{
	ext2_ino_t ino;
	struct walking_inode_data dump_needed;
	const char *dname;
	int res;

	if (!fs || !in_dp || !count)
		return -1;

	*count = 0;
	if (!directory_exists(out_dp)) {
		LOGE("dir need dump into an existed dir\n");
		return -1;
	}

	res = e2fs_get_inodenum_by_fpath(fs, in_dp, &ino);
	if (res == -1)
		return -1;

	dname = strrchr(in_dp, '/');
	if (dname)
		dname++;
	else
		dname = in_dp;

	dump_needed.dumped_count = 0;
	dump_needed.current_out_native_dirpath = out_dp;
	res = dump_inode_recursively_by_inodenum(fs, ino, &dump_needed, dname);
	*count = dump_needed.dumped_count;
	if (res) {
		LOGE("ext2fs failed to dump dir\n");
		return -1;
	}

	return 0;
}

int e2fs_open(const char *dev, ext2_filsys *outfs)
{
	errcode_t res;

	if (!dev || !outfs)
		return -1;

	add_error_table(&et_ext2_error_table);
	res = ext2fs_open(dev, EXT2_FLAG_64BITS, 0, 0,
			  unix_io_manager, outfs);
	if (res) {
		LOGE("ext2fs fail to open (%s), error (%s)\n", dev,
		     error_message(res));
		return -1;
	}

	return 0;
}

void e2fs_close(ext2_filsys fs)
{
	if (fs)
		ext2fs_close(fs);
	remove_error_table(&et_ext2_error_table);
}
