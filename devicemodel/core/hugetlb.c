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
#include <fcntl.h>
#include <errno.h>

#include "vmmapi.h"

extern char *vmname;

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

#define SYS_PATH_LV1  "/sys/kernel/mm/hugepages/hugepages-2048kB/"
#define SYS_PATH_LV2  "/sys/kernel/mm/hugepages/hugepages-1048576kB/"
#define SYS_NR_HUGEPAGES  "nr_hugepages"
#define SYS_FREE_HUGEPAGES  "free_hugepages"

/* hugetlb_info record private information for one specific hugetlbfs:
 * - mounted: is hugetlbfs mounted for below mount_path
 * - mount_path: hugetlbfs mount path
 * - mount_opt: hugetlb mount option
 * - node_path: record for hugetlbfs node path
 * - pg_size: this hugetlbfs's page size
 * - lowmem: lowmem of this hugetlbfs need allocate
 * - highmem: highmem of this hugetlbfs need allocate
 *.- pages_delta: its value equals needed pages - free pages,
 *.---if > 0: it's the gap for needed page; if < 0, more free than needed.
 * - nr_pages_path: sys path for total number of pages
 *.- free_pages_path: sys path for number of free pages
 */
struct hugetlb_info {
	bool mounted;
	char *mount_path;
	char *mount_opt;

	char node_path[MAX_PATH_LEN];
	int fd;
	int pg_size;
	size_t lowmem;
	size_t biosmem;
	size_t highmem;

	int pages_delta;
	char *nr_pages_path;
	char *free_pages_path;
};

static struct hugetlb_info hugetlb_priv[HUGETLB_LV_MAX] = {
	{
		.mounted = false,
		.mount_path = PATH_HUGETLB_LV1,
		.mount_opt = OPT_HUGETLB_LV1,
		.fd = -1,
		.pg_size = 0,
		.lowmem = 0,
		.biosmem = 0,
		.highmem = 0,

		.pages_delta = 0,
		.nr_pages_path = SYS_PATH_LV1 SYS_NR_HUGEPAGES,
		.free_pages_path = SYS_PATH_LV1 SYS_FREE_HUGEPAGES,
	},
	{
		.mounted = false,
		.mount_path = PATH_HUGETLB_LV2,
		.mount_opt = OPT_HUGETLB_LV2,
		.fd = -1,
		.pg_size = 0,
		.lowmem = 0,
		.biosmem = 0,
		.highmem = 0,

		.pages_delta = 0,
		.nr_pages_path = SYS_PATH_LV2 SYS_NR_HUGEPAGES,
		.free_pages_path = SYS_PATH_LV2 SYS_FREE_HUGEPAGES,
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
	size_t len;
	struct statfs fs;

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	path = hugetlb_priv[level].node_path;
	memset(path, '\0', MAX_PATH_LEN);
	snprintf(path, MAX_PATH_LEN, "%s%s/", hugetlb_priv[level].mount_path, ctx->name);

	len = strnlen(path, MAX_PATH_LEN);
	/* UUID will use 32 bytes */
	if (len + 32 > MAX_PATH_LEN) {
		perror("PATH overflow");
		return -ENOMEM;
	}

	uuid_copy(UUID, ctx->vm_uuid);
	snprintf(uuid_str, sizeof(uuid_str),
		"%02X%02X%02X%02X%02X%02X%02X%02X"
		"%02X%02X%02X%02X%02X%02X%02X%02X",
		UUID[0], UUID[1], UUID[2], UUID[3],
		UUID[4], UUID[5], UUID[6], UUID[7],
		UUID[8], UUID[9], UUID[10], UUID[11],
		UUID[12], UUID[13], UUID[14], UUID[15]);

	*(path + len) = '\0';
	strncat(path, uuid_str, strnlen(uuid_str, sizeof(uuid_str)));

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
	        hugetlb_priv[level].biosmem > 0 ||
	        hugetlb_priv[level].highmem > 0);
}

/*
 * level  : hugepage level
 * len	  : region length for mmap
 * offset : region start offset from ctx->baseaddr
 * skip   : skip offset in different level hugetlbfs fd
 */
static int mmap_hugetlbfs_from_level(struct vmctx *ctx, int level, size_t len,
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

static int mmap_hugetlbfs(struct vmctx *ctx, size_t offset,
		void (*get_param)(struct hugetlb_info *, size_t *, size_t *),
		size_t (*adj_param)(struct hugetlb_info *, struct hugetlb_info *, int))
{
	size_t len, skip;
	int level, ret = 0, pg_size;

	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		get_param(&hugetlb_priv[level], &len, &skip);
		pg_size = hugetlb_priv[level].pg_size;

		while (len > 0) {
			ret = mmap_hugetlbfs_from_level(ctx, level, len, offset, skip);

			if (ret < 0 && level > HUGETLB_LV1) {
				len = adj_param(
						&hugetlb_priv[level], &hugetlb_priv[level-1],
						pg_size);
			} else if (ret < 0 && level == HUGETLB_LV1) {
				goto done;
			} else {
				offset += len;
				break;
			}
		}
	}

done:
	return ret;
}

static void get_lowmem_param(struct hugetlb_info *htlb,
		size_t *len, size_t *skip)
{
	*len = htlb->lowmem;
	*skip = 0; /* nothing to skip as lowmen is mmap'ed first */
}

static size_t adj_lowmem_param(struct hugetlb_info *htlb,
		struct hugetlb_info *htlb_prev, int adj_size)
{
	htlb->lowmem -= adj_size;
	htlb_prev->lowmem += adj_size;

	return htlb->lowmem;
}

static void get_highmem_param(struct hugetlb_info *htlb,
		size_t *len, size_t *skip)
{
	*len = htlb->highmem;
	*skip = htlb->lowmem;
}

static size_t adj_highmem_param(struct hugetlb_info *htlb,
		struct hugetlb_info *htlb_prev, int adj_size)
{
	htlb->highmem -= adj_size;
	htlb_prev->highmem += adj_size;

	return htlb->highmem;
}

static void get_biosmem_param(struct hugetlb_info *htlb,
		size_t *len, size_t *skip)
{
	*len = htlb->biosmem;
	*skip = htlb->lowmem + htlb->highmem;
}

static size_t adj_biosmem_param(struct hugetlb_info *htlb,
		struct hugetlb_info *htlb_prev, int adj_size)
{
	htlb->biosmem -= adj_size;
	htlb_prev->biosmem += adj_size;

	return htlb->biosmem;
}

static int rm_hugetlb_dirs(int level)
{
	char path[MAX_PATH_LEN]={0};

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	snprintf(path,MAX_PATH_LEN, "%s%s/",hugetlb_priv[level].mount_path,vmname);

	if (access(path, F_OK) == 0) {
		if (rmdir(path) < 0) {
			perror("rmdir failed");
			return -1;
		}
	}
	return 0;
}

static int create_hugetlb_dirs(int level)
{
	char path[MAX_PATH_LEN]={0};
	int i;
	size_t len;

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	snprintf(path,MAX_PATH_LEN, "%s%s/",hugetlb_priv[level].mount_path,vmname);

	len = strnlen(path, MAX_PATH_LEN);
	for (i = 1; i < len; i++) {
		if (path[i] == '/') {
			path[i] = 0;
			if (access(path, F_OK) != 0) {
				if (mkdir(path, 0755) < 0) {
					perror("mkdir failed");
					return -1;
				}
			}
			path[i] = '/';
		}
	}

	return 0;
}

static int mount_hugetlbfs(int level)
{
	int ret;
	char path[MAX_PATH_LEN];

	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return -EINVAL;
	}

	if (hugetlb_priv[level].mounted)
		return 0;

	snprintf(path, MAX_PATH_LEN, "%s%s", hugetlb_priv[level].mount_path,vmname);

	/* only support x86 as HUGETLB level-1 2M page, level-2 1G page*/
	ret = mount("none", path, "hugetlbfs",
		0, hugetlb_priv[level].mount_opt);
	if (ret == 0)
		hugetlb_priv[level].mounted = true;

	return ret;
}

static void umount_hugetlbfs(int level)
{
	char path[MAX_PATH_LEN];
	
	if (level >= HUGETLB_LV_MAX) {
		perror("exceed max hugetlb level");
		return;
	}

	snprintf(path, MAX_PATH_LEN, "%s%s", hugetlb_priv[level].mount_path,vmname);


	if (hugetlb_priv[level].mounted) {
		umount(path);
		hugetlb_priv[level].mounted = false;
	}
}

static int read_sys_info(const char *sys_path)
{
	FILE *fp;
	char tmp_buf[12];
	int pages = 0;
	int result;

	fp = fopen(sys_path, "r");
	if (fp == NULL) {
		printf("can't open: %s, err: %s\n", sys_path, strerror(errno));
		return 0;
	}

	memset(tmp_buf, 0, 12);
	result = fread(&tmp_buf, sizeof(char), 8, fp);
	if (result <= 0)
		printf("read %s, error: %s, please check!\n",
			sys_path, strerror(errno));
	else
		pages = strtol(tmp_buf, NULL, 10);

	fclose(fp);
	return pages;
}

/* check if enough free huge pages for the UOS */
static bool hugetlb_check_memgap(void)
{
	int lvl, free_pages, need_pages;
	bool has_gap = false;

	for (lvl = HUGETLB_LV1; lvl < hugetlb_lv_max; lvl++) {
		free_pages = read_sys_info(hugetlb_priv[lvl].free_pages_path);
		need_pages = (hugetlb_priv[lvl].lowmem + hugetlb_priv[lvl].biosmem +
			hugetlb_priv[lvl].highmem) / hugetlb_priv[lvl].pg_size;

		hugetlb_priv[lvl].pages_delta = need_pages - free_pages;
		/* if delta > 0, it's a gap for needed pages, to be handled */
		if (hugetlb_priv[lvl].pages_delta > 0)
			has_gap = true;

		printf("level %d free/need pages:%d/%d page size:0x%x\n", lvl,
			free_pages, need_pages, hugetlb_priv[lvl].pg_size);
	}

	return has_gap;
}

/* try to reserve more huge pages on the level */
static void reserve_more_pages(int level)
{
	int total_pages, orig_pages, cur_pages;
	char cmd_buf[MAX_PATH_LEN];
	FILE *fp;

	orig_pages = read_sys_info(hugetlb_priv[level].nr_pages_path);
	total_pages = orig_pages + hugetlb_priv[level].pages_delta;
	snprintf(cmd_buf, MAX_PATH_LEN, "echo %d > %s",
		total_pages, hugetlb_priv[level].nr_pages_path);

	/* system cmd to reserve needed huge pages */
	fp = popen(cmd_buf, "r");
	if (fp == NULL) {
		printf("cmd: %s failed!\n", cmd_buf);
		return;
	}
	pclose(fp);

	printf("to reserve pages (+orig %d): %s\n", orig_pages, cmd_buf);
	cur_pages = read_sys_info(hugetlb_priv[level].nr_pages_path);
	hugetlb_priv[level].pages_delta = total_pages - cur_pages;
}

/* try to release larger free page */
static bool release_larger_freepage(int level_limit)
{
	int level;
	int total_pages, orig_pages, cur_pages;
	char cmd_buf[MAX_PATH_LEN];
	FILE *fp;

	for (level = hugetlb_lv_max - 1; level >= level_limit; level--) {
		if (hugetlb_priv[level].pages_delta >= 0)
			continue;

		/* free one unsed larger page */
		orig_pages = read_sys_info(hugetlb_priv[level].nr_pages_path);
		total_pages = orig_pages - 1;
		snprintf(cmd_buf, MAX_PATH_LEN, "echo %d > %s",
			total_pages, hugetlb_priv[level].nr_pages_path);

		fp = popen(cmd_buf, "r");
		if (fp == NULL) {
			printf("cmd to free mem: %s failed!\n", cmd_buf);
			return false;
		}
		pclose(fp);

		cur_pages = read_sys_info(hugetlb_priv[level].nr_pages_path);

		/* release page successfully */
		if (cur_pages < orig_pages) {
			hugetlb_priv[level].pages_delta++;
			break;
		}
	}

	if (level < level_limit)
		return false;

	return true;
}

/* reserve more free huge pages as different levels.
 * it need handle following cases:
 * A.no enough free memory to reserve gap pages, just fails.
 * B enough free memory to reserve each level gap pages
 *.C.enough free memory, but it can't reserve enough higher level gap pages,
 *    so lower level need handle that, to reserve more free pages.
 *.D.enough higher level free pages, but not enough free memory for
 *    lower level gap pages, so release some higher level free pages for that.
 * other info:
 *.   even enough free memory, it is eaiser to reserve smaller pages than
 * lager ones, for example:2MB easier than 1GB. One flow of current solution:
 *.it could leave SOS very small free memory.
 *.return value: true: success; false: failure
 */
static bool hugetlb_reserve_pages(void)
{
	int left_gap = 0, pg_size;
	int level;

	printf("to reserve more free pages:\n");
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		if (hugetlb_priv[level].pages_delta <= 0)
			continue;

		/* if gaps, try to reserve more pages */
		reserve_more_pages(level);

		/* check if reserved enough pages */
		if (hugetlb_priv[level].pages_delta <= 0)
			continue;

		/* probably system allocates fewer pages than needed
		 * especially for larger page like 1GB, even there is enough
		 * free memory, it stil can fail to allocate 1GB huge page.
		 * so if that,it needs the next level to handle it.
		 */
		if (level > HUGETLB_LV1) {
			left_gap = hugetlb_priv[level].pages_delta;
			pg_size = hugetlb_priv[level].pg_size;
			hugetlb_priv[level - 1].pages_delta += (size_t)left_gap
				* (pg_size / hugetlb_priv[level - 1].pg_size);
			continue;
		}

		/* now for level == HUGETLB_LV1 it still can't alloc enough
		 * pages, go back to larger pages, to check if it can
		 *.release some unused free pages, if release success,
		 * let it go LV1 again
		 */
		if (release_larger_freepage(level + 1))
			level++;
		else
			break;
	}

	if (level >= HUGETLB_LV1) {
		printf("level %d pages gap: %d failed to reserve!\n",
			level, left_gap);
		return false;
	}

	printf("now enough free pages are reserved!\n");
	return true;
}


bool init_hugetlb(void)
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

void uninit_hugetlb(void)
{
	int level;
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		umount_hugetlbfs(level);
		rm_hugetlb_dirs(level);
	}
}

int hugetlb_setup_memory(struct vmctx *ctx)
{
	int level;
	size_t lowmem, biosmem, highmem;
	bool has_gap;

	if (ctx->lowmem == 0) {
		perror("vm requests 0 memory");
		goto err;
	}

	/* open hugetlbfs and get pagesize for two level */
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		if (open_hugetlbfs(ctx, level) < 0) {
			perror("failed to open hugetlbfs");
			goto err;
		}
	}

	/* all memory should be at least aligned with
	 * hugetlb_priv[HUGETLB_LV1].pg_size */
	ctx->lowmem =
		ALIGN_DOWN(ctx->lowmem, hugetlb_priv[HUGETLB_LV1].pg_size);
	ctx->biosmem =
		ALIGN_DOWN(ctx->biosmem, hugetlb_priv[HUGETLB_LV1].pg_size);
	ctx->highmem =
		ALIGN_DOWN(ctx->highmem, hugetlb_priv[HUGETLB_LV1].pg_size);

	total_size = ctx->highmem_gpa_base + ctx->highmem;

	/* check & set hugetlb level memory size for lowmem/biosmem/highmem */
	lowmem = ctx->lowmem;
	biosmem = ctx->biosmem;
	highmem = ctx->highmem;

	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		hugetlb_priv[level].lowmem =
			ALIGN_DOWN(lowmem, hugetlb_priv[level].pg_size);
		hugetlb_priv[level].biosmem =
			ALIGN_DOWN(biosmem, hugetlb_priv[level].pg_size);
		hugetlb_priv[level].highmem =
			ALIGN_DOWN(highmem, hugetlb_priv[level].pg_size);

		if (level > HUGETLB_LV1) {
			hugetlb_priv[level-1].lowmem = lowmem =
				lowmem - hugetlb_priv[level].lowmem;
			hugetlb_priv[level-1].biosmem = biosmem =
				biosmem - hugetlb_priv[level].biosmem;
			hugetlb_priv[level-1].highmem = highmem =
				highmem - hugetlb_priv[level].highmem;
		}
	}

	/* it will check each level memory need */
	has_gap = hugetlb_check_memgap();
	if (has_gap) {
		if (!hugetlb_reserve_pages())
			goto err;
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
		printf("\tlevel %d - lowmem 0x%lx, biosmem 0x%lx, highmem 0x%lx\n",
			level,
			hugetlb_priv[level].lowmem,
			hugetlb_priv[level].biosmem,
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
	if (mmap_hugetlbfs(ctx, 0, get_lowmem_param, adj_lowmem_param) < 0) {
		perror("lowmem mmap failed");
		goto err;
	}

	/* mmap highmem */
	if (mmap_hugetlbfs(ctx, ctx->highmem_gpa_base,
				get_highmem_param, adj_highmem_param) < 0) {
		perror("highmem mmap failed");
		goto err;
	}

	/* mmap biosmem */
	if (mmap_hugetlbfs(ctx, 4 * GB - ctx->biosmem,
				get_biosmem_param, adj_biosmem_param) < 0) {
		perror("biosmem mmap failed");
		goto err;
	}

	/* dump hugepage really setup */
	printf("\nreally setup hugepage with:\n");
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		printf("\tlevel %d - lowmem 0x%lx, biosmem 0x%lx, highmem 0x%lx\n",
			level,
			hugetlb_priv[level].lowmem,
			hugetlb_priv[level].biosmem,
			hugetlb_priv[level].highmem);
	}

	/* map ept for lowmem */
	if (vm_map_memseg_vma(ctx, ctx->lowmem, 0,
		(uint64_t)ctx->baseaddr, PROT_ALL) < 0)
		goto err;

	/* map ept for biosmem */
	if (ctx->biosmem > 0) {
		if (vm_map_memseg_vma(ctx, ctx->biosmem, 4 * GB - ctx->biosmem,
			(uint64_t)(ctx->baseaddr + 4 * GB - ctx->biosmem),
			PROT_READ | PROT_EXEC) < 0)
		goto err;
	}

	/* map ept for highmem */
	if (ctx->highmem > 0) {
		if (vm_map_memseg_vma(ctx, ctx->highmem, ctx->highmem_gpa_base,
			(uint64_t)(ctx->baseaddr + ctx->highmem_gpa_base),
			PROT_ALL) < 0)
			goto err;
	}

	return 0;

err:
	if (ptr) {
		munmap(ptr, total_size);
		ptr = NULL;
	}
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		close_hugetlbfs(level);
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
	}
}
