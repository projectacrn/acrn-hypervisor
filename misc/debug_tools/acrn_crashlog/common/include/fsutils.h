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

#ifndef __FSUTILS_H__
#define __FSUTILS_H__

#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#define MAX(a, b)                (((a) > (b)) ? (a) : (b))
#define MIN(a, b)                (((a) > (b)) ? (b) : (a))

#define KB                      (1024)
#define MB                      (KB * KB)
#define MAXLINESIZE             (PATH_MAX + 128)
#define CPBUFFERSIZE            (4 * KB)
#define PAGE_SIZE               (4 * KB)

struct mm_file_t {
	char *path;
	int fd;
	char *begin;
	ssize_t size;
};

struct ac_filter_data {
	const char *str;
	size_t len;
};

static inline int file_exists(const char *filename)
{
	struct stat info;

	return (stat(filename, &info) == 0);
}

static inline int directory_exists(const char *path)
{
	struct stat info;

	return (stat(path, &info) == 0 && S_ISDIR(info.st_mode));
}

static inline ssize_t get_file_size(const char *filepath)
{
	struct stat info;

	if (filepath == NULL)
		return -ENOENT;

	if (stat(filepath, &info) < 0)
		return -errno;

	return info.st_size;
}

static inline ssize_t get_file_blocks_size(const char *filepath)
{
	struct stat info;

	if (filepath == NULL)
		return -ENOENT;

	if (stat(filepath, &info) < 0)
		return -errno;

	return info.st_blocks * 512;
}

char *mm_get_line(struct mm_file_t *mfile, int line);
int mkdir_p(const char *path);
int remove_r(const char *dir);
int mm_count_lines(struct mm_file_t *mfile);
struct mm_file_t *mmap_file(const char *path);
void unmap_file(struct mm_file_t *mfile);
int do_copy_tail(const char *src, const char *dest, int limit);
int do_mv(char *src, char *dest);
ssize_t append_file(const char *filename, const char *text, size_t tlen);
int replace_file_head(char *filename, char *text);
int overwrite_file(const char *filename, const char *value);
int readline(int fd, char buffer[MAXLINESIZE]);
ssize_t file_read_string(const char *file, char *string, const int size);
void file_reset_init(const char *filename);
int file_read_int(const char *filename, unsigned int *pcurrent);
int file_update_int(const char *filename, unsigned int current,
			unsigned int max);
int do_copy_limit(const char *src, const char *des, size_t limitsize);
int space_available(const char *path, int quota);
int count_lines_in_file(const char *filename);
int read_full_binary_file(const char *path, unsigned long *size,
			void **data);
ssize_t file_read_key_value(char *value, const size_t limit, const char *path,
			const char *key, size_t klen);
ssize_t file_read_key_value_r(char *value, const size_t limit, const char *path,
			const char *key, size_t klen);
int ac_scandir(const char *dirp, struct dirent ***namelist,
		int (*filter)(const struct dirent *, const void *),
		const void *farg,
		int (*compar)(const struct dirent **,
				const struct dirent **));
int filter_filename_substr(const struct dirent *entry, const void *arg);
int filter_filename_exactly(const struct dirent *entry, const void *arg);
int filter_filename_startswith(const struct dirent *entry,
					const void *arg);
int dir_contains(const char *dir, const char *filename, size_t flen, int exact);
int lsdir(const char *dir, char *fullname[], int limit);
int find_file(const char *dir, size_t dlen, const char *target_file,
		size_t tflen, int depth, char *path[], int limit);
int dir_blocks_size(const char *dir, size_t dlen, size_t *size);
int read_file(const char *path, unsigned long *size, void **data);
int is_ac_filefmt(const char *file_fmt);
int config_fmt_to_files(const char *file_fmt, char ***out);

#endif
