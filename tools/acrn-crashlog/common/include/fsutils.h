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
#define MAXLINESIZE             (4 * KB)
#define CPBUFFERSIZE            (4 * KB)
#define PAGE_SIZE               (4 * KB)

struct mm_file_t {
	char *path;
	int fd;
	char *begin;
	int size;
};

static inline int file_exists(const char *filename)
{
	struct stat info;

	return (stat(filename, &info) == 0);
}

static inline int directory_exists(char *path)
{
	struct stat info;

	return (stat(path, &info) == 0 && S_ISDIR(info.st_mode));
}

static inline int get_file_size(const char *filepath)
{
	struct stat info;

	if (filepath == NULL)
		return -ENOENT;

	if (stat(filepath, &info) < 0)
		return -errno;

	return info.st_size;
}

char *mm_get_line(struct mm_file_t *mfile, int line);
int mkdir_p(char *path);
int mm_count_lines(struct mm_file_t *mfile);
struct mm_file_t *mmap_file(const char *path);
void unmap_file(struct mm_file_t *mfile);
int do_copy_tail(char *src, char *dest, int limit);
int do_mv(char *src, char *dest);
int append_file(char *filename, char *text);
int mm_replace_str_line(struct mm_file_t *mfile, char *replace,
			int line);
int replace_file_head(char *filename, char *text);
int overwrite_file(char *filename, char *value);
int readline(int fd, char buffer[MAXLINESIZE]);
int file_read_string(const char *file, char *string, int size);
void file_reset_init(const char *filename);
int file_read_int(const char *filename, unsigned int *pcurrent);
int file_update_int(const char *filename, unsigned int current,
			unsigned int max);
int do_copy_eof(const char *src, const char *des);
int space_available(char *path, int quota);
int count_lines_in_file(const char *filename);
int read_full_binary_file(const char *path, unsigned long *size,
			void **data);
int file_read_key_value(char *path, char *key, char *value);
int file_read_key_value_r(char *path, char *key, char *value);
int dir_contains(const char *dir, const char *filename, int exact,
		char *fullname);
int lsdir(const char *dir, char *fullname[], int limit);
int find_file(char *dir, char *target_file, int depth, char *path[], int limit);
int read_file(const char *path, unsigned long *size, void **data);

#endif
