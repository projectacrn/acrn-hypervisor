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
#include "fsutils.h"
#include "cmdutils.h"
#include "strutils.h"
#include "log_sys.h"

#define MAX_SEARCH_DIRS 4096

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
int mkdir_p(char *path)
{
	if (!path)
		return -EINVAL;

	/* return 0 if path exists */
	return exec_out2file(NULL, "mkdir -p %s", path);
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
	else if (line == 1)
		ret = begin;
	else {
		next = begin;
		for (i = 2; i <= line; i++)
			next = strchr(next, '\n') + 1;
		ret = next;
	}

	return ret;
}

/**
 * Replace the content in specified line.
 *
 * @param mfile File opened by mmap_file.
 * @param replace New content.
 * @param line Target line.
 *
 * @return 0 lines if successful, or a negative errno-style value if not.
 */
int mm_replace_str_line(struct mm_file_t *mfile, char *replace, int line)
{
	int len, rmlen, offset;
	int res;
	int move_size = 0;
	char *add_buf;
	char *to_replace;
	int oldsize = mfile->size;
	int fd = mfile->fd;

	len = strlen(replace);
	add_buf = malloc(len + 1);
	if (add_buf == NULL)
		return -ENOMEM;

	if (replace[len - 1] != '\n')
		sprintf(add_buf, "%s\n", replace);
	else
		sprintf(add_buf, "%s", replace);
	len = strlen(add_buf);

	to_replace = mm_get_line(mfile, line);
	if (to_replace == NULL) {
		LOGE("no line %d in %s\n", line, mfile->path);
		free(add_buf);
		return -EINVAL;
	}
	rmlen = strlinelen(to_replace);
	offset = len - rmlen;

	/* resize the file and add/del the space to add_buf */
	if (offset > 0) {
		int newsize = oldsize + offset;

		/* shift right */
		res = ftruncate(fd, newsize);
		if (res < 0) {
			free(add_buf);
			return -errno;
		}

		if ((newsize / PAGE_SIZE) > (oldsize / PAGE_SIZE)) {
			/* the size crosses PAGESIZE, we need remap */
			char *old_addr;

			old_addr = mfile->begin;
			mfile->begin = mremap(old_addr, oldsize, newsize,
					      MREMAP_MAYMOVE);
			if (mfile->begin == MAP_FAILED) {
				free(add_buf);
				return -errno;
			}
			/* refresh the target place after remap */
			to_replace = mm_get_line(mfile, line);
		}

		mfile->size = newsize;
		move_size = mfile->begin + oldsize - to_replace;
		memmove(to_replace + offset, to_replace, move_size);
	} else if (offset < 0) {
		/* shift left the next line */
		move_size = mfile->begin + oldsize - to_replace - rmlen;
		memmove(to_replace + len, to_replace + rmlen, move_size);
		mfile->size += offset;

		res = ftruncate(fd, oldsize + offset);
		if (res < 0) {
			free(add_buf);
			return -errno;
		}
	} /* else we don't need shift */

	memcpy(to_replace, add_buf, len);
	free(add_buf);

	return 0;
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
	int size;

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
	size = mfile->size > 0 ? mfile->size : PAGE_SIZE;
	mfile->begin = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED,
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
int do_copy_tail(char *src, char *dest, int limit)
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
int append_file(char *filename, char *text)
{
	int fd, res, len;

	if (!filename || !text)
		return -EINVAL;

	len = strlen(text);
	if (!len)
		return -EINVAL;

	fd = open(filename, O_RDWR | O_APPEND);
	if (fd < 0)
		return -errno;

	res = write(fd, text, len);
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
 * @param text String need to be appended.
 *
 * @return 0 if successful, or a negative errno-style value if not.
 */
int overwrite_file(char *filename, char *value)
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
 * Read line from file descriptor.
 *
 * @param fd File descriptor.
 * @param[out] buffer Content of line.
 *
 * @return length of line if successful, or a negative errno-style value if not.
 */
int readline(int fd, char buffer[MAXLINESIZE])
{
	int size = 0, res;
	char *pbuffer = &buffer[0];

	/* Read the file until end of line or file */
	while ((res = read(fd, pbuffer, 1)) == 1 && size < MAXLINESIZE-1) {
		if (pbuffer[0] == '\n') {
			buffer[++size] = 0;
			return size;
		}
		pbuffer++;
		size++;
	}

	/* Check the last read result */
	if (res < 0) {
		/* ernno is checked in the upper layer as we could
		 * print the filename here
		 */
		return res;
	}
	/* last line */
	buffer[size] = 0;

	return size;
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
int file_read_string(const char *file, char *string, int size)
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
	return strlen(string);
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
