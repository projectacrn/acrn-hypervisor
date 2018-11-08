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

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/sendfile.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <ftw.h>
#include "fsutils.h"
#include "cmdutils.h"
#include "strutils.h"
#include "log_sys.h"

/**
 * Help function to do fclose. In some cases (full partition),
 * file closure could fail, so get error message here.
 *
 * @param filename Path of file.
 * @param fp FILE pointer open by fopen.
 *
 * @return Upon successful completion 0 is returned.
 *         Otherwise, EOF is returned and errno is set to indicate the error.
 */
static int close_file(const char *filename, FILE *fp)
{
	int res;

	errno = 0;
	res = fclose(fp);
	if (res != 0) {
		/* File closure could fail in some cases (full partition) */
		LOGE("fclose on %s failed - error is %s\n",
			filename, strerror(errno));
	}

	return res;
}

/**
 * Create the dir, make parent directories as needed.
 *
 * @param path Target dir to make.
 * @param mode Dir mode.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
int mkdir_p(const char *path)
{
	if (!path)
		return -EINVAL;

	/* return 0 if path exists */
	return exec_out2file(NULL, "mkdir -p %s", path);
}

static int rmfile(const char *path,
		const struct stat *sbuf __attribute__((unused)),
		int type __attribute__((unused)),
		struct FTW *ftwb __attribute__((unused)))
{
	if (remove(path) == -1) {
		LOGE("failed to remove (%s), error (%s)\n", path,
		     strerror(errno));
		return -1;
	}
	return 0;
}

int remove_r(const char *dir)
{
	if (!dir)
		return -1;

	if (nftw(dir, rmfile, 10, FTW_DEPTH | FTW_MOUNT | FTW_PHYS) == -1)
		return -1;

	return 0;
}

/**
 * Count file lines.
 * This function defaults to all text files ending with \n.
 *
 * @param mfile File opened by mmap_file.
 *
 * @return file lines if successful, or a negative errno-style value if not.
 */
int mm_count_lines(struct mm_file_t *mfile)
{
	if (!mfile || mfile->size < 0)
		return -EINVAL;

	if (!mfile->size)
		return 0;

	return strcnt(mfile->begin, '\n');
}

/**
 * Get the head of specified line from area mapped by mmap_file.
 *
 * @param mfile File opened by mmap_file.
 * @param line Target line.
 *
 * @return a pointer to the head of line if successful, or NULL if not.
 */
char *mm_get_line(struct mm_file_t *mfile, int line)
{
	char *begin = mfile->begin;
	char *next;
	int i;
	char *ret;

	if (line <= 0 || line > mm_count_lines(mfile))
		return NULL;

	if (line == 1)
		ret = begin;
	else {
		next = begin;
		for (i = 2; i <= line && next; i++)
			next = strchr(next, '\n') + 1;
		ret = next;
	}

	return ret;
}

/**
 * Open a file and map it. The structure mm_file_t maintains the major
 * infomations of this file.
 *
 * @param path File path need mmap.
 *
 * @return a pointer to the structure mm_file_t if succuessful, or NULL if not.
 */
struct mm_file_t *mmap_file(const char *path)
{
	struct mm_file_t *mfile;

	mfile = malloc(sizeof(struct mm_file_t));
	if (!mfile) {
		LOGE("malloc failed, error (%s)\n", strerror(errno));
		return NULL;
	}

	mfile->path = strdup(path);
	if (!mfile->path) {
		LOGE("strdup failed, error (%s)\n", strerror(errno));
		goto free_mfile;
	}

	mfile->fd = open(mfile->path, O_RDWR);
	if (mfile->fd < 0) {
		LOGE("open (%s) failed, error (%s)\n", path, strerror(errno));
		goto free_path;
	}

	mfile->size = get_file_size(path);
	if (mfile->size < 0) {
		LOGE("get filesize of (%s) failed, error (%s)\n",
		     path, strerror(errno));
		goto close_fd;
	}
	mfile->begin = mmap(NULL, mfile->size, PROT_READ|PROT_WRITE, MAP_SHARED,
			    mfile->fd, 0);
	if (mfile->begin == MAP_FAILED) {
		LOGE("mmap (%s) failed, error (%s)\n", path, strerror(errno));
		goto close_fd;
	}

	return mfile;

close_fd:
	close(mfile->fd);
free_path:
	free(mfile->path);
free_mfile:
	free(mfile);
	return NULL;

}

/**
 * Close and unmap the file opened by mmap_file.
 *
 * @param mfile File opened by mmap_file.
 */
void unmap_file(struct mm_file_t *mfile)
{
	close(mfile->fd);
	munmap(mfile->begin, mfile->size);
	free(mfile->path);
	free(mfile);
}

/**
 * Copy the tail data from a file which supports mmap(2)-like operations
 * to new file.
 *
 * @param src File path to copy, this file supports mmap(2)-like operations
 *            (i.e., it cannot be a socket).
 * @param dest New file path to generate.
 * @param limit Size of data, if limit equals 0, this function
 *	  will copy entire file.
 *
 * @return The number of bytes written to new file if successful,
 *         or a negative errno-style value if not.
 */
int do_copy_tail(const char *src, const char *dest, int limit)
{
	int rc = 0;
	int fsrc = -1, fdest = -1;
	struct stat info;
	off_t offset = 0;

	if (src == NULL || dest == NULL)
		return -EINVAL;

	if (stat(src, &info) < 0)
		return -errno;

	fsrc = open(src, O_RDONLY);
	if (fsrc < 0)
		return -errno;

	fdest = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if (fdest < 0) {
		close(fsrc);
		return -errno;
	}

	if ((limit == 0) || (info.st_size < limit))
		limit = info.st_size;

	if (info.st_size > limit)
		offset = info.st_size - limit;

	rc = sendfile(fdest, fsrc, &offset, limit);

	close(fsrc);
	close(fdest);

	return rc == -1 ? -errno : rc;
}

/**
 * Move a file.
 *
 * @param src Old file path.
 * @param dest New file path.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
int do_mv(char *src, char *dest)
{
	struct stat info;

	if (src == NULL || dest == NULL)
		return -EINVAL;

	if (stat(src, &info) < 0)
		return -errno;

	/* check if destination exists */
	if (stat(dest, &info)) {
		/* an error, unless the destination was missing */
		if (errno != ENOENT) {
			LOGE("failed on '%s', err:%s", dest, strerror(errno));
			return -errno;
		}
	}
	/* attempt to move it */
	if (rename(src, dest)) {
		LOGE("failed on '%s', err:%s\n", src, strerror(errno));
		return -errno;
	}

	return 0;
}

/**
 * Add a string to the end of file.
 *
 * @param filename Path of file.
 * @param text String need to be appended.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
ssize_t append_file(const char *filename, const char *text, size_t tlen)
{
	int fd, res;

	if (!filename || !text || !tlen)
		return -EINVAL;

	fd = open(filename, O_WRONLY | O_APPEND);
	if (fd < 0)
		return -errno;

	res = write(fd, text, tlen);
	close(fd);

	return (res == -1 ? -errno : res);
}

/**
 * Replace a string from the beginning of file.
 *
 * @param filename Path of file.
 * @param text String need to replace.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
int replace_file_head(char *filename, char *text)
{
	FILE *fp;

	fp = fopen(filename, "r+");
	if (fp == NULL)
		return -errno;

	errno = 0;
	fputs(text, fp);
	close_file(filename, fp);
	if (errno != 0)
		return -errno;

	return 0;
}

/**
 * Change the entire file content to specified string.
 * The file is created if it does not exist, otherwise it is truncated.
 *
 * @param filename Path of file.
 * @param text String need to be written.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
int overwrite_file(const char *filename, const char *value)
{
	FILE *fp;
	int ret = 0;

	if (!filename || !value)
		return -EINVAL;

	fp = fopen(filename, "w+");
	if (fp == NULL)
		return -errno;

	if (fprintf(fp, "%s", value) <= 0)
		ret = -errno;

	close_file(filename, fp);

	return ret;
}

/**
 * Read the first line from file.
 *
 * @param file Path of file.
 * @param[out] string String read out.
 * @param[out] size Size of string.
 *
 * @return length of string if successful, or a negative errno-style value
 *	   if not.
 */
ssize_t file_read_string(const char *file, char *string, const int size)
{
	FILE *fp;
	char *res;
	char *end;

	if (!file || !string)
		return -EINVAL;

	if (!file_exists(file))
		return -ENOENT;

	fp = fopen(file, "r");
	if (!fp)
		return -errno;

	res = fgets(string, size, fp);

	close_file(file, fp);

	if (!res)
		return -errno;

	end = strchr(string, '\n');
	if (end)
		*end = 0;
	return strnlen(string, size);
}

/**
 * Reset the string in file to zero.
 *
 * @param filename Path of file.
 */
void file_reset_init(const char *filename)
{
	FILE *fp;

	fp = fopen(filename, "w");
	if (fp == NULL) {
		LOGE("Cannot reset %s - %s\n", filename,
		    strerror(errno));
		return;
	}
	fprintf(fp, "%4u", 0);
	close_file(filename, fp);
}

/**
 * Read the string in file and convert it to type int.
 *
 * @param filename Path of file.
 * @param[out] pcurrent The result read out.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
int file_read_int(const char *filename, unsigned int *pcurrent)
{
	FILE *fp;
	int res;

	/* Open file in reading */
	fp = fopen(filename, "r");
	if (fp != NULL) {
		/* Read current value from file */
		res = fscanf(fp, "%4u", pcurrent);
		if (res != 1) {
			/* Set it to 0 by default */
			*pcurrent = 0;
			LOGE("read KO res=%d, current=%d - error is %s\n",
			     res, *pcurrent, strerror(errno));
			res = 0;
		}
		/* Close file */
		res = close_file(filename, fp);
	} else if (errno == ENOENT) {
		LOGI("File %s does not exist, fall back to folder 0.\n",
		     filename);
		/* Initialize file */
		file_reset_init(filename);
		*pcurrent = 0;
		res = 0;
	} else {
		LOGE("Cannot open file %s - error is %s.\n", filename,
		     strerror(errno));
		res = -errno;
	}

	return res;
}

/**
 * Update the string in file to specified value.
 *
 * @param filename Path of file.
 * @param current The number to record.
 * @param max The max value file can record, this function would record
 *        remainder((current+1) % max)) if the specified value exceeds max.
 *
 * @return 0 if successful, or a negative value if not.
 */
int file_update_int(const char *filename, unsigned int current,
		    unsigned int max)
{
	FILE *fp;
	int res;

	/* Open file in reading and writing */
	fp = fopen(filename, "r+");
	if (fp == NULL) {
		LOGE("Cannot open the file %s in update mode\n", filename);
		return -1;
	}

	/* Write new current value in file */
	res = fprintf(fp, "%4u", ((current + 1) % max));
	if (res <= 0) {
		LOGE("Cannot update file %s - error is %s.\n",
			filename, strerror(errno));
		/* Close file */
		close_file(filename, fp);
		return -1;
	}

	/* Close file */
	return close_file(filename, fp);
}

/**
 * Copy file in a read/write loop.
 *
 * @param src Path of source file.
 * @param dest Path of destin file.
 *
 * @return 0 if successful, or a negative value if not.
 */
int do_copy_eof(const char *src, const char *des)
{
	char buffer[CPBUFFERSIZE];
	int rc = 0;
	int fd1 = -1, fd2 = -1;
	struct stat info;
	int r_count, w_count = 0;

	if (src == NULL || des == NULL)
		return -EINVAL;

	if (stat(src, &info) < 0) {
		LOGE("can not open file: %s\n", src);
		return -errno;
	}

	fd1 = open(src, O_RDONLY);
	if (fd1 < 0)
		return -errno;

	fd2 = open(des, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if (fd2 < 0) {
		LOGE("can not open file: %s\n", des);
		close(fd1);
		return -errno;
	}

	/* Start copy loop */
	while (1) {
		/* Read data from src */
		r_count = read(fd1, buffer, CPBUFFERSIZE);
		if (r_count < 0) {
			LOGE("read failed, err:%s\n", strerror(errno));
			rc = -1;
			break;
		}

		if (r_count == 0)
			break;

		/* Copy data to des */
		w_count = write(fd2, buffer, r_count);
		if (w_count < 0) {
			LOGE("write failed, err:%s\n", strerror(errno));
			rc = -1;
			break;
		}
		if (r_count != w_count) {
			LOGE("write failed, r_count:%d w_count:%d\n",
			     r_count, w_count);
			rc = -1;
			break;
		}
	}

	if (fd1 >= 0)
		close(fd1);
	if (fd2 >= 0)
		close(fd2);

	return rc;
}

/**
 * Check the storage space.
 *
 * @param path Path to the file system.
 * @param quota Threshold value.
 * @return 1 if the percentage of using space is lower than the specified quota.
 *         or 0 if not.
 */
int space_available(const char *path, int quota)
{
	struct statfs diskInfo;
	unsigned long long totalBlocks;
	unsigned long long totalSize;
	unsigned long long freeDisk;
	int mbTotalsize;
	int mbFreedisk;
	int ret;

	/* quota is valid in range (0, 100) */
	if (quota <= 0 || quota > 100)
		return 0;

	ret = statfs(path, &diskInfo);
	if (ret < 0) {
		LOGE("statfs (%s) failed, error (%s)\n", path,
		     strerror(errno));
		return 0;
	}
	totalBlocks = diskInfo.f_bsize;
	totalSize = totalBlocks * diskInfo.f_blocks;
	freeDisk = diskInfo.f_bfree * totalBlocks;
	mbTotalsize = totalSize >> 20;
	mbFreedisk = freeDisk >> 20;
	if ((float)mbFreedisk / (float)mbTotalsize >
	    1 - ((float)quota / (float)100))
		return 1;

	LOGE("space meet quota[%d] total=%dMB, free=%dMB\n", quota,
	     mbTotalsize, mbFreedisk);
	return 0;
}

/**
 * Count file lines.
 *
 * @param filename Path of file to count lines.
 *
 * @return lines of file is successful, or a negative errno-style value if not.
 */
int count_lines_in_file(const char *filename)
{
	struct mm_file_t *f;
	int ret;

	f = mmap_file(filename);
	if (!f)
		return -errno;

	ret = mm_count_lines(f);

	unmap_file(f);

	return ret;
}

static ssize_t _file_read_key_value(char *value, const size_t limit,
				const char *path, const char *key,
				size_t klen, const char op)
{
	size_t len;
	char *msg = NULL;
	struct mm_file_t *f = mmap_file(path);

	if (!f)
		return -errno;
	if (!key) {
		errno = EINVAL;
		return -errno;
	}

	if (op != 'l' && op != 'r') {
		errno = EINVAL;
		return -errno;
	}

	if (op == 'l')
		msg = get_line(key, klen, f->begin, f->size, f->begin, &len);
	else if (op == 'r') {
		char *msg_tmp;

		msg = f->begin;
		len = 0;
		while ((msg_tmp = get_line(key, klen, f->begin, f->size,
					  msg, &len)))
			msg = msg_tmp + len;
		if (msg != f->begin)
			msg -= len;
		else
			msg = NULL;
	}
	if (!msg) {
		errno = ENOMSG;
		goto unmap;
	}

	len = MIN(len - klen, limit - 1);
	*(char *)mempcpy(value, msg + klen, len) = '\0';

	unmap_file(f);
	return len;

unmap:
	unmap_file(f);
	return -errno;
}

ssize_t file_read_key_value(char *value, const size_t limit, const char *path,
			const char *key, size_t klen)
{
	return _file_read_key_value(value, limit, path, key, klen, 'l');
}

ssize_t file_read_key_value_r(char *value, const size_t limit, const char *path,
			const char *key, size_t klen)
{
	return _file_read_key_value(value, limit, path, key, klen, 'r');
}

/**
 * Because scandir's filter can't receive caller's parameter, so
 * rewrite an ac_scandir to satisfy our usage. This function is
 * very like scandir, except it has an additional parameter farg for
 * filter.
 *
 * @param dirp Dir to scan.
 * @param namelist Andress to receive result array of struct dirent.
 * @param filter Function pointer to filter. See also scandir.
 * @param farg The second arg of filter.
 * @param compar See scandir.
 *
 * @return the count of scanned files on success, or -1 on error.
 */
int ac_scandir(const char *dirp, struct dirent ***namelist,
		int (*filter)(const struct dirent *, const void *),
		const void *farg,
		int (*compar)(const struct dirent **,
				const struct dirent **))
{
	int i;
	int count = 0;
	int index = 0;
	struct dirent **_filelist;
	struct dirent **_outlist;

	if (!dirp || !namelist)
		return -1;

	const int res = scandir(dirp, &_filelist, NULL, compar);

	if (!filter) {
		*namelist = _filelist;
		return res;
	}
	if (res == -1) {
		LOGE("failed to scandir, error (%s)\n", strerror(errno));
		return -1;
	}

	/* overwrite filter */
	/* calculate the matched files, free unneeded files and mark them */
	for (i = 0; i < res; i++) {
		if (!filter(_filelist[i], farg)) {
			count++;
		} else {
			free(_filelist[i]);
			_filelist[i] = 0;
		}
	}

	/* no matched result */
	if (!count) {
		free(_filelist);
		return 0;
	}

	/* construct the out array */
	_outlist = malloc(count * sizeof(struct dirent *));
	if (!_outlist) {
		LOGE("failed to malloc\n");
		goto e_free;
	}

	for (i = 0; i < res; i++) {
		if (_filelist[i])
			_outlist[index++] = _filelist[i];
	}

	free(_filelist);

	*namelist = _outlist;
	return count;

e_free:
	for (i = 0; i < res; i++)
		if (_filelist[i])
			free(_filelist[i]);
	free(_filelist);
	return -1;
}

/* filters return zero if the match is successful */
int filter_filename_substr(const struct dirent *entry, const void *arg)
{
	struct ac_filter_data *d = (struct ac_filter_data *)arg;

	return !memmem(entry->d_name, _D_EXACT_NAMLEN(entry), d->str, d->len);
}

int filter_filename_exactly(const struct dirent *entry, const void *arg)
{
	struct ac_filter_data *d = (struct ac_filter_data *)arg;

	return strcmp(entry->d_name, d->str);
}

int filter_filename_startswith(const struct dirent *entry,
				const void *arg)
{
	struct ac_filter_data *d = (struct ac_filter_data *)arg;

	if (_D_EXACT_NAMLEN(entry) < d->len)
		return -1;

	return memcmp(entry->d_name, d->str, d->len);
}

int dir_contains(const char *dir, const char *filename, size_t flen,
		const int exact)
{
	int ret;
	int i;
	struct dirent **filelist;
	struct ac_filter_data acfd = {filename, flen};

	if (!dir || !filename || !flen)
		return -1;

	if (exact)
		ret = ac_scandir(dir, &filelist, filter_filename_exactly,
				 (const void *)&acfd, 0);
	else
		ret = ac_scandir(dir, &filelist, filter_filename_substr,
				 (const void *)&acfd, 0);
	if (ret <= 0)
		return ret;

	for (i = 0; i < ret; i++)
		free(filelist[i]);

	free(filelist);

	return ret;
}

int lsdir(const char *dir, char *fullname[], int limit)
{
	int ret, num, count = 0;
	int i;
	struct dirent **filelist;
	char *name;

	if (!dir || !fullname)
		return -EINVAL;

	num = scandir(dir, &filelist, 0, 0);
	if (num < 0)
		return -errno;
	if (num > limit) {
		LOGE("(%s) contains (%d) files, meet limit (%d)\n",
		     dir, num, limit);
		count = -EINVAL;
		goto free_list;
	}

	for (i = 0; i < num; i++) {
		name = filelist[i]->d_name;
		ret = asprintf(&fullname[count++], "%s/%s", dir, name);
		if (ret < 0) {
			LOGE("compute string failed, out of memory\n");
			count = -ENOMEM;
			goto free_fname;
		}
	}

	for (i = 0; i < num; i++)
		free(filelist[i]);
	free(filelist);

	return count;

free_fname:
	for (i = 0; i < count; i++)
		free(fullname[i]);

free_list:
	for (i = 0; i < num; i++)
		free(filelist[i]);
	free(filelist);

	return count;
}

#define DIR_SUCCESS (0)
#define DIR_ABORT (-1)
#define DIR_ERROR (-2)
static int dir_recursive(const char *path, size_t plen, int depth,
			int fn(const char *pdir, struct dirent *, void *arg),
			void *arg)
{
	int wdepth = 0;
	int ret = DIR_SUCCESS;
	char wdir[PATH_MAX];
	DIR **dp;

	if (!path)
		return DIR_ERROR;
	if (plen >= PATH_MAX)
		return DIR_ERROR;
	if (depth < -1)
		return DIR_ERROR;

	if (depth == -1)
		depth = 1024;

	*(char *)mempcpy(wdir, path, plen) = '\0';
	dp = calloc(depth + 1, sizeof(DIR *));
	if (!dp) {
		LOGE("out of memory\n");
		return DIR_ERROR;
	}

	while (wdepth >= 0) {
		struct dirent *dirp;

		if (!dp[wdepth]) {
			/* new stream */
			dp[wdepth] = opendir(wdir);
			if (!dp[wdepth]) {
				LOGE("failed to opendir (%s), error (%s)\n",
				     wdir, strerror(errno));
				ret = DIR_ERROR;
				goto fail_open;
			}
		}

		while (!(errno = 0) && (dirp = readdir(dp[wdepth]))) {
			if (!strcmp(dirp->d_name, ".") ||
			    !strcmp(dirp->d_name, ".."))
				continue;

			ret = fn(wdir, dirp, arg);
			if (ret == DIR_ABORT)
				goto end;
			if (ret == DIR_ERROR)
				goto fail_read;

			if (dirp->d_type == DT_DIR && wdepth < depth) {
				/* search in subdir */
				*(char *)
				mempcpy(mempcpy(wdir + strnlen(wdir, PATH_MAX),
					"/", 1),
					dirp->d_name,
					strnlen(dirp->d_name, NAME_MAX)) = '\0';
				wdepth++;
				break;
			}
		}
		if (!dirp) {
			if (!errno) {
				/* meet the end of stream, back to the parent */
				char *p;

				closedir(dp[wdepth]);
				dp[wdepth--] = NULL;
				p = strrchr(wdir, '/');
				if (p)
					*p = '\0';
			} else {
				LOGE("failed to readdir, (%s)\n",
				     strerror(errno));
				ret = DIR_ERROR;
				goto fail_read;
			}
		}
	}
end:
	while (wdepth >= 0)
		closedir(dp[wdepth--]);
	free(dp);
	return ret;

fail_read:
	LOGE("failed to search in dir %s\n", wdir);
	closedir(dp[wdepth]);
fail_open:
	while (wdepth)
		closedir(dp[--wdepth]);
	free(dp);
	return ret;

}

struct find_file_data {
	const char *target;
	size_t tlen;
	int limit;
	char **path;
	int found;
};

static int _get_file_path(const char *pdir, struct dirent *dirp, void *arg)
{
	struct find_file_data *d = (struct find_file_data *)arg;

	if (!strcmp(dirp->d_name, d->target)) {
		if (asprintf(&d->path[d->found], "%s/%s", pdir,
		    dirp->d_name) == -1) {
			LOGE("out of memory\n");
			return DIR_ERROR;
		}
		if (++(d->found) == d->limit)
			return DIR_ABORT;
	}

	return DIR_SUCCESS;
}

/**
 * Find target file in specified dir.
 *
 * @param dir Where to start search.
 * @param dlen Length of dir.
 * @param target_file Target file to search.
 * @param tflen The length of target_file.
 * @param depth Descend at most depth of directories below the starting dir.
 * @param path[out] Searched file path in given dir.
 * @param limit The number of files uplayer want to get.
 *
 * @return the count of searched files on success, or -1 on error.
 */
int find_file(const char *dir, size_t dlen, const char *target_file,
		size_t tflen, int depth, char *path[], int limit)
{
	int res;
	struct find_file_data data = {
		.target = target_file,
		.tlen = tflen,
		.limit = limit,
		.path = path,
		.found = 0,
	};

	if (!dir || !target_file || !tflen || !path || limit <= 0)
		return -1;

	res = dir_recursive(dir, dlen, depth, _get_file_path, (void *)&data);
	if (res == DIR_ABORT || res == DIR_SUCCESS)
		return data.found;

	if (res == DIR_ERROR) {
		while (data.found)
			free(path[--data.found]);
	}
	return -1;
}

static int _count_file_size(const char *pdir, struct dirent *dirp, void *arg)
{
	char file[PATH_MAX];
	int res;
	ssize_t fsize;

	if (dirp->d_type != DT_REG && dirp->d_type != DT_DIR)
		return DIR_SUCCESS;

	res = snprintf(file, sizeof(file), "%s/%s", pdir, dirp->d_name);
	if (s_not_expect(res, sizeof(file)))
		return DIR_ERROR;

	fsize = get_file_size(file);
	if (fsize < 0)
		return DIR_ERROR;

	*(size_t *)arg += fsize;

	return DIR_SUCCESS;
}

int dir_size(const char *dir, size_t dlen, size_t *size)
{
	if (!dir || !dlen || !size)
		return -1;

	*size = 0;
	if (dir_recursive(dir, dlen, -1, _count_file_size,
			  (void *)size) != DIR_SUCCESS) {
		LOGE("failed to recursive dir (%s)\n", dir);
		return -1;
	}

	return 0;
}

int read_file(const char *path, unsigned long *size, void **data)
{
	char tmp[1024];
	int len = 0;
	int fd = 0;
	int memsize = 1; /* for '\0' */
	ssize_t result = 0;
	char *out = NULL;
	char *new;

	if (!path || !data || !size) {
		errno = EINVAL;
		return -1;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	while (1) {
		result = read(fd, (void *)tmp, 1024);
		if (result == 0)
			break;
		else if (result == -1)
			goto free;

		memsize += result;
		new = realloc(out, memsize);
		if (!new)
			goto free;

		out = new;
		memcpy(out + len, tmp, result);
		out[memsize - 1] = 0;

		len += result;
	}

	close(fd);

	*data = out;
	*size = (unsigned long)len;

	return 0;

free:
	if (out)
		free(out);
	close(fd);
	return -1;
}

int is_ac_filefmt(const char *file_fmt)
{
	/* Supported formats:
	 * - /dir/.../file[*] --> all files with prefix "file."
	 * - /dir/.../file[0] --> file with smallest subfix num.
	 * - /dir/.../file[-1] --> file with biggest subfix num.
	 */
	if (!file_fmt)
		return 0;

	return (strstr(file_fmt, "[*]") ||
		strstr(file_fmt, "[0]") ||
		strstr(file_fmt, "[-1]"));
}

/**
 * The config file of acrnprobe could use some format to indicate a file/files.
 * This function is used to parse the format and returns found files paths.
 *
 * @param file_fmt A string pointer of a file format.
 * @param out Files were found.
 *
 * @return the count of searched files on success, or -1 on error.
 */
int config_fmt_to_files(const char *file_fmt, char ***out)
{
	char type[3];
	char *dir;
	char *p;
	char *subfix;
	char *file_prefix;
	int i;
	int count;
	int res = 0;
	int ret = 0;
	struct dirent **filelist;
	char **out_array;
	struct ac_filter_data acfd;

	if (!file_fmt || !out)
		return -1;

	dir = strdup(file_fmt);
	if (!dir) {
		LOGE("failed to strdup\n");
		return -1;
	}

	if (!is_ac_filefmt(file_fmt)) {
		/* It's an regular file as default */
		out_array = malloc(sizeof(char *));
		if (!out_array) {
			ret = -1;
			goto free_dir;
		}

		out_array[0] = dir;
		*out = out_array;
		return 1;
	}

	/* get dir and file prefix from format */
	p = strrchr(dir, '/');
	if (!p) {
		LOGE("only support abs path, dir (%s)\n", file_fmt);
		ret = -1;
		goto free_dir;
	}
	*p = '\0';
	file_prefix = p + 1;
	p = strrchr(file_prefix, '[');
	if (!p) {
		ret = -1;
		LOGE("unsupported formats (%s)\n", file_fmt);
		goto free_dir;
	}

	*p = '\0';
	acfd.str = file_prefix;
	acfd.len = p - file_prefix;

	if (!directory_exists(dir)) {
		ret = 0;
		goto free_dir;
	}
	/* get format type */
	subfix = strrchr(file_fmt, '[');
	if (!subfix) {
		ret = -1;
		LOGE("unsupported formats (%s)\n", file_fmt);
		goto free_dir;
	}
	p = memccpy(type, subfix + 1, ']', 3);
	if (!p) {
		ret = -1;
		LOGE("unsupported formats (%s)\n", file_fmt);
		goto free_dir;
	} else
		*(p - 1) = '\0';

	/* get all files which start with prefix */
	count = ac_scandir(dir, &filelist, filter_filename_startswith,
			   &acfd, alphasort);
	if (count < 0) {
		ret = -1;
		LOGE("failed to ac_scandir\n");
		goto free_dir;
	}
	if (!count) {
		ret = 0;
		goto free_dir;
	}

	/* construct output */
	out_array = (char **)malloc(count * sizeof(char *));
	if (!out_array) {
		ret = -1;
		LOGE("failed to malloc\n");
		goto free_filelist;
	}

	if (!strcmp(type, "*")) {
		for (i = 0; i < count; i++) {
			res = asprintf(&out_array[i], "%s/%s", dir,
				       filelist[i]->d_name);
			if (res == -1) {
				/* free from 0 to i -1 */
				while (i)
					free(out_array[--i]);
				break;
			}
		}
		ret = count;
	} else if (!strcmp(type, "0")) {
		res = asprintf(&out_array[0], "%s/%s", dir,
			       filelist[0]->d_name);
		ret = 1;
	} else if (!strcmp(type, "-1")) {
		res = asprintf(&out_array[0], "%s/%s", dir,
			       filelist[count - 1]->d_name);
		ret = 1;
	}

	/* error happends while constructing output */
	if (res == -1) {
		ret = -errno;
		free(out_array);
		goto free_filelist;
	}

	*out = out_array;

free_filelist:
	for (i = 0; i < count; i++)
		free(filelist[i]);
	free(filelist);
free_dir:
	free(dir);

	return ret;
}
