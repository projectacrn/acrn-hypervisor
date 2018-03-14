/*-
 * Copyright (c) 2018 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "vmm.h"
#include "vhm_ioctl_defs.h"
#include "vmmapi.h"

#define HUGETLB_LV1		0
#define HUGETLB_LV2		1
#define HUGETLB_LV_MAX	2

#define MAX_PATH_LEN 128

#define HUGETLBFS_MAGIC       0x958458f6

/* HugePage Level 1 for 2M page, Level 2 for 1G page*/
#define PATH_HUGETLB_LV1 "/run/hugepage/acrn/huge_lv1/"
#define OPT_HUGETLB_LV1 "pagesize=2M"
#define PATH_HUGETLB_LV2 "/run/hugepage/acrn/huge_lv2/"
#define OPT_HUGETLB_LV2 "pagesize=1G"

/* hugetlb_info record private information for one specific hugetlbfs:
 * - mounted: is hugetlbfs mounted for below mount_path
 * - mount_path: hugetlbfs mount path
 * - mount_opt: hugetlb mount option
 * - node_path: record for hugetlbfs node path
 * - pg_size: this hugetlbfs's page size
 * - lowmem: lowmem of this hugetlbfs need allocate
 * - highmem: highmem of this hugetlbfs need allocate
 */
struct hugetlb_info {
	bool mounted;
	char *mount_path;
	char *mount_opt;

	char node_path[MAX_PATH_LEN];
	int fd;
	int pg_size;
	size_t lowmem;
	size_t highmem;
};

static struct hugetlb_info hugetlb_priv[HUGETLB_LV_MAX] = {
	{
		.mounted = false,
		.mount_path = PATH_HUGETLB_LV1,
		.mount_opt = OPT_HUGETLB_LV1,
		.fd = -1,
		.pg_size = 0,
		.lowmem = 0,
		.highmem = 0,
	},
	{
		.mounted = false,
		.mount_path = PATH_HUGETLB_LV2,
		.mount_opt = OPT_HUGETLB_LV2,
		.fd = -1,
		.pg_size = 0,
		.lowmem = 0,
		.highmem = 0,
	},
};

static void *ptr;
static size_t total_size;
static int hugetlb_lv_max;

static int open_hugetlbfs(struct vmctx *ctx, int level)
{
	char uuid_str[48];
	uint8_t	 UUID[16];
	char *path;
	struct statfs fs;

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	path = hugetlb_priv[level].node_path;
	strncpy(path, hugetlb_priv[level].mount_path, MAX_PATH_LEN);

	/* UUID will use 32 bytes */
	if (strlen(path) + 32 > MAX_PATH_LEN) {
		perror("PATH overflow");
		return -ENOMEM;
	}

	uuid_copy(UUID, ctx->vm_uuid);
	sprintf(uuid_str, "%02X%02X%02X%02X%02X%02X%02X%02X"
		"%02X%02X%02X%02X%02X%02X%02X%02X\n",
		UUID[0], UUID[1], UUID[2], UUID[3],
		UUID[4], UUID[5], UUID[6], UUID[7],
		UUID[8], UUID[9], UUID[10], UUID[11],
		UUID[12], UUID[13], UUID[14], UUID[15]);

	strncat(path, uuid_str, strlen(uuid_str));

	printf("open hugetlbfs file %s\n", path);

	hugetlb_priv[level].fd = open(path, O_CREAT | O_RDWR, 0644);
	if (hugetlb_priv[level].fd  < 0) {
		perror("Open hugtlbfs failed");
		return -EINVAL;
	}

	/* get the pagesize */
	if (fstatfs(hugetlb_priv[level].fd, &fs) != 0) {
		perror("Failed to get statfs fo hugetlbfs");
		return -EINVAL;
	}

	if (fs.f_type == HUGETLBFS_MAGIC) {
		/* get hugepage size from fstat*/
		hugetlb_priv[level].pg_size = fs.f_bsize;
	} else {
		close(hugetlb_priv[level].fd);
		unlink(hugetlb_priv[level].node_path);
		hugetlb_priv[level].fd = -1;
		return -EINVAL;
	}

	return 0;
}

static void close_hugetlbfs(int level)
{
	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return;
	}

	if (hugetlb_priv[level].fd >= 0) {
		close(hugetlb_priv[level].fd);
		hugetlb_priv[level].fd = -1;
		unlink(hugetlb_priv[level].node_path);
		hugetlb_priv[level].pg_size = 0;
	}
}

static bool should_enable_hugetlb_level(int level)
{
	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return false;
	}

	return (hugetlb_priv[level].lowmem > 0 ||
		hugetlb_priv[level].highmem > 0);
}

/*
 * level  : hugepage level
 * len	  : region length for mmap
 * offset : region start offset from ctx->baseaddr
 * skip   : skip offset in different level hugetlbfs fd
 */
static int mmap_hugetlbfs(struct vmctx *ctx, int level, size_t len,
		size_t offset, size_t skip)
{
	char *addr;
	size_t pagesz = 0;
	int fd, i;

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	fd = hugetlb_priv[level].fd;
	addr = mmap(ctx->baseaddr + offset, len, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, fd, skip);
	if (addr == MAP_FAILED)
		return -ENOMEM;

	printf("mmap 0x%lx@%p\n", len, addr);

	/* pre-allocate hugepages by touch them */
	pagesz = hugetlb_priv[level].pg_size;

	printf("touch %ld pages with pagesz 0x%lx\n", len/pagesz, pagesz);

	for (i = 0; i < len/pagesz; i++) {
		*(volatile char *)addr = *addr;
		addr += pagesz;
	}

	return 0;
}

static int mmap_hugetlbfs_lowmem(struct vmctx *ctx)
{
	size_t len, offset, skip;
	int level, ret = 0, pg_size;

	offset = skip = 0;
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		len = hugetlb_priv[level].lowmem;
		pg_size = hugetlb_priv[level].pg_size;
		while (len > 0) {
			ret = mmap_hugetlbfs(ctx, level, len, offset, skip);
			if (ret < 0 && level > HUGETLB_LV1) {
				len -= pg_size;
				hugetlb_priv[level].lowmem = len;
				hugetlb_priv[level-1].lowmem += pg_size;
			} else if (ret < 0 && level == HUGETLB_LV1)
				return ret;
			else {
				offset += len;
				break;
			}
		}
	}

	return 0;
}

static int mmap_hugetlbfs_highmem(struct vmctx *ctx)
{
	size_t len, offset, skip;
	int level, ret = 0, pg_size;

	offset = 4 * GB;
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		skip = hugetlb_priv[level].lowmem;
		len = hugetlb_priv[level].highmem;
		pg_size = hugetlb_priv[level].pg_size;
		while (len > 0) {
			ret = mmap_hugetlbfs(ctx, level, len, offset, skip);
			if (ret < 0 && level > HUGETLB_LV1) {
				len -= pg_size;
				hugetlb_priv[level].highmem = len;
				hugetlb_priv[level-1].highmem += pg_size;
			} else if (ret < 0 && level == HUGETLB_LV1)
				return ret;
			else {
				offset += len;
				break;
			}
		}
	}

	return 0;
}

static int create_hugetlb_dirs(int level)
{
	char tmp_path[MAX_PATH_LEN], *path;
	int i, len;

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	path = hugetlb_priv[level].mount_path;
	len = strlen(path);
	if (len >= MAX_PATH_LEN) {
		perror("exceed max path len");
		return -EINVAL;
	}

	strcpy(tmp_path, path);

	if (tmp_path[len - 1] != '/')
		strcat(tmp_path, "/");

	len = strlen(tmp_path);
	for (i = 1; i < len; i++) {
		if (tmp_path[i] == '/') {
			tmp_path[i] = 0;
			if (access(tmp_path, F_OK) != 0) {
				if (mkdir(tmp_path, 0755) < 0) {
					perror("mkdir failed");
					return -1;
				}
			}
			tmp_path[i] = '/';
		}
	}

	return 0;
}

static int mount_hugetlbfs(int level)
{
	int ret;

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	if (hugetlb_priv[level].mounted)
		return 0;

	/* only support x86 as HUGETLB level-1 2M page, level-2 1G page*/
	ret = mount("none", hugetlb_priv[level].mount_path, "hugetlbfs",
		0, hugetlb_priv[level].mount_opt);
	if (ret == 0)
		hugetlb_priv[level].mounted = true;

	return ret;
}

static void umount_hugetlbfs(int level)
{
	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return;
	}

	if (hugetlb_priv[level].mounted) {
		umount(hugetlb_priv[level].mount_path);
		hugetlb_priv[level].mounted = false;
	}
}

bool check_hugetlb_support(void)
{
	int level;

	for (level = HUGETLB_LV1; level < HUGETLB_LV_MAX; level++) {
		if (create_hugetlb_dirs(level) < 0)
			return false;
	}

	for (level = HUGETLB_LV1; level < HUGETLB_LV_MAX; level++) {
		if (mount_hugetlbfs(level) < 0) {
			level--;
			break;
		}
	}

	if (level < HUGETLB_LV1) /* mount fail for level 1 */
		return false;
	else if (level == HUGETLB_LV1) /* mount fail for level 2 */
		printf("WARNING: only level 1 hugetlb supported");

	hugetlb_lv_max = level;

	return true;
}

int hugetlb_setup_memory(struct vmctx *ctx)
{
	int level;
	size_t lowmem, highmem;

	/* for first time DM start UOS, hugetlbfs is already mounted by
	 * check_hugetlb_support; but for reboot, here need re-mount
	 * it as it already be umount by hugetlb_unsetup_memory
	 * TODO: actually, correct reboot process should not change memory
	 * layout, the setup_memory should be removed from reboot process
	 */
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++)
		mount_hugetlbfs(level);

	/* open hugetlbfs and get pagesize for two level */
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		if (open_hugetlbfs(ctx, level) < 0) {
			perror("failed to open hugetlbfs");
			goto err;
		}
	}

	/* all memory should be at least align with
	 * hugetlb_priv[HUGETLB_LV1].pg_size */
	ctx->lowmem =
		ALIGN_DOWN(ctx->lowmem, hugetlb_priv[HUGETLB_LV1].pg_size);
	ctx->highmem =
		ALIGN_DOWN(ctx->highmem, hugetlb_priv[HUGETLB_LV1].pg_size);

	if (ctx->highmem > 0)
		total_size = 4 * GB + ctx->highmem;
	else
		total_size = ctx->lowmem;

	if (total_size == 0) {
		perror("vm request 0 memory");
		goto err;
	}

	/* check & set hugetlb level memory size for lowmem & highmem */
	highmem = ctx->highmem;
	lowmem = ctx->lowmem;
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		hugetlb_priv[level].lowmem =
			ALIGN_DOWN(lowmem, hugetlb_priv[level].pg_size);
		hugetlb_priv[level].highmem =
			ALIGN_DOWN(highmem, hugetlb_priv[level].pg_size);
		if (level > HUGETLB_LV1) {
			hugetlb_priv[level-1].lowmem = lowmem =
				lowmem - hugetlb_priv[level].lowmem;
			hugetlb_priv[level-1].highmem = highmem =
				highmem - hugetlb_priv[level].highmem;
		}
	}

	/* align up total size with huge page size for vma alignment */
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		if (should_enable_hugetlb_level(level)) {
			total_size += hugetlb_priv[level].pg_size;
			break;
		}
	}

	/* dump hugepage trying to setup */
	printf("\ntry to setup hugepage with:\n");
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		printf("\tlevel %d - lowmem 0x%lx, highmem 0x%lx\n", level,
			hugetlb_priv[level].lowmem,
			hugetlb_priv[level].highmem);
	}
	printf("total_size 0x%lx\n\n", total_size);

	/* basic overview vma */
	ptr = mmap(NULL, total_size, PROT_NONE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("anony mmap fail");
		goto err;
	}

	/* align up baseaddr according to hugepage level size */
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		if (should_enable_hugetlb_level(level)) {
			ctx->baseaddr = (void *)ALIGN_UP((size_t)ptr,
						hugetlb_priv[level].pg_size);
			break;
		}
	}
	printf("mmap ptr 0x%p -> baseaddr 0x%p\n", ptr, ctx->baseaddr);

	/* mmap lowmem */
	if (mmap_hugetlbfs_lowmem(ctx) < 0)
		goto err;

	/* mmap highmem */
	if (mmap_hugetlbfs_highmem(ctx) < 0)
		goto err;

	/* dump hugepage really setup */
	printf("\nreally setup hugepage with:\n");
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		printf("\tlevel %d - lowmem 0x%lx, highmem 0x%lx\n", level,
			hugetlb_priv[level].lowmem,
			hugetlb_priv[level].highmem);
	}
	printf("total_size 0x%lx\n\n", total_size);

	return 0;

err:
	if (ptr) {
		munmap(ptr, total_size);
		ptr = NULL;
	}
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		close_hugetlbfs(level);
		umount_hugetlbfs(level);
	}
	return -ENOMEM;
}

void hugetlb_unsetup_memory(struct vmctx *ctx)
{
	int level;

	if (total_size > 0) {
		munmap(ptr, total_size);
		total_size = 0;
		ptr = NULL;
	}

	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		close_hugetlbfs(level);
		umount_hugetlbfs(level);
	}
}
