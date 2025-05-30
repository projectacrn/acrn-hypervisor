/*-
 * Copyright (c) 2018-2022 Intel Corporation.
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
#include <log.h>
#include <linux/memfd.h>

#include "vmmapi.h"

extern char *vmname;

#define ALIGN_CHECK(x, align)	(((x) & ((align)-1)) ? 1 : 0)

#define HUGETLB_LV1		0
#define HUGETLB_LV2		1
#define HUGETLB_LV_MAX		2

#define MAX_PATH_LEN 256

/* HugePage Level 1 for 2M page, Level 2 for 1G page*/

#define SYS_PATH_LV1  "/sys/kernel/mm/hugepages/hugepages-2048kB/"
#define SYS_PATH_LV2  "/sys/kernel/mm/hugepages/hugepages-1048576kB/"
#define SYS_NR_HUGEPAGES  "nr_hugepages"
#define SYS_FREE_HUGEPAGES  "free_hugepages"

/* File used for lock between different processes access to hugetlbfs.
 * We observed when access hugetlbfs from different process to allocate
 * huge page at the same time could fail. So use file lock here to make
 * sure hugetlbfs is accessed sequentially.
 *
 * We use file range (0..9) for hugetlbfs access lock.
 */
#define	ACRN_HUGETLB_LOCK_DIR  "/run/hugepage/acrn"
#define	ACRN_HUGETLB_LOCK_FILE "/run/hugepage/acrn/lock"
#define	LOCK_OFFSET_START	0
#define	LOCK_OFFSET_END		10

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
	int fd;
	int pg_size;
	size_t lowmem;
	size_t fbmem;
	size_t biosmem;
	size_t highmem;
	unsigned int flags;

	int pages_delta;
	char *nr_pages_path;
	char *free_pages_path;
};

static struct hugetlb_info hugetlb_priv[HUGETLB_LV_MAX] = {
	{
		.fd = -1,
		.pg_size = 2048 * 1024,
		.lowmem = 0,
		.fbmem = 0,
		.biosmem = 0,
		.highmem = 0,
		.flags = MFD_CLOEXEC | MFD_ALLOW_SEALING |
			 MFD_HUGETLB | MFD_HUGE_2MB,

		.pages_delta = 0,
		.nr_pages_path = SYS_PATH_LV1 SYS_NR_HUGEPAGES,
		.free_pages_path = SYS_PATH_LV1 SYS_FREE_HUGEPAGES,
	},
	{
		.fd = -1,
		.pg_size = 1024 * 1024 * 1024,
		.lowmem = 0,
		.fbmem = 0,
		.biosmem = 0,
		.highmem = 0,
		.flags = MFD_CLOEXEC | MFD_ALLOW_SEALING |
			 MFD_HUGETLB | MFD_HUGE_1GB,

		.pages_delta = 0,
		.nr_pages_path = SYS_PATH_LV2 SYS_NR_HUGEPAGES,
		.free_pages_path = SYS_PATH_LV2 SYS_FREE_HUGEPAGES,
	},
};

struct vm_mmap_mem_region {
	vm_paddr_t gpa_start;
	vm_paddr_t gpa_end;
	vm_paddr_t fd_offset;
	char *hva_base;
	int fd;
};

static struct vm_mmap_mem_region mmap_mem_regions[16];
static int mem_idx;

static void *ptr;
static size_t total_size;
static int hugetlb_lv_max;
static int lock_fd;

static int lock_acrn_hugetlb(void)
{
	int ret;

	ret = lockf(lock_fd, F_LOCK, LOCK_OFFSET_END);

	if (ret < 0) {
		pr_err("lock acrn hugetlb failed with errno: %d\n", errno);
		return ret;
	}

	return 0;
}

static int unlock_acrn_hugetlb(void)
{
	int ret;

	ret = lockf(lock_fd, F_ULOCK, LOCK_OFFSET_END);

	if (ret < 0) {
		pr_err("lock acrn hugetlb failed with errno: %d\n", errno);
		return ret;
	}

	return 0;
}


static void close_hugetlbfs(int level)
{
	if (level >= HUGETLB_LV_MAX) {
		pr_err("exceed max hugetlb level");
		return;
	}

	if (hugetlb_priv[level].fd >= 0) {
		close(hugetlb_priv[level].fd);
		hugetlb_priv[level].fd = -1;
	}
}

static bool should_enable_hugetlb_level(int level)
{
	if (level >= HUGETLB_LV_MAX) {
		pr_err("exceed max hugetlb level");
		return false;
	}
	if (hugetlb_priv[level].fd < 0)
		return false;

	return (hugetlb_priv[level].lowmem > 0 ||
		hugetlb_priv[level].fbmem > 0 ||
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
		size_t offset, size_t skip, char **addr_out)
{
	char *addr;
	size_t pagesz = 0;
	int fd, i;

	if (level >= HUGETLB_LV_MAX) {
		pr_err("exceed max hugetlb level");
		return -EINVAL;
	}
	if (mem_idx >= ARRAY_SIZE(mmap_mem_regions)) {
		pr_err("exceed supported regions.\n");
		return -EFAULT;
	}

	fd = hugetlb_priv[level].fd;
	addr = mmap(ctx->baseaddr + offset, len, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_FIXED, fd, skip);
	if (addr == MAP_FAILED)
		return -ENOMEM;

	if (addr_out)
		*addr_out = addr;

	/* add the mapping into mmap_mem_region */
	mmap_mem_regions[mem_idx].gpa_start = offset;
	mmap_mem_regions[mem_idx].gpa_end = offset + len;
	mmap_mem_regions[mem_idx].fd = fd;
	mmap_mem_regions[mem_idx].fd_offset = skip;
	mmap_mem_regions[mem_idx].hva_base = addr;
	mem_idx++;
	pr_info("mmap 0x%lx@%p\n", len, addr);

	/* pre-allocate hugepages by touch them */
	pagesz = hugetlb_priv[level].pg_size;

	pr_info("touch %ld pages with pagesz 0x%lx\n", len/pagesz, pagesz);

	/* Access to the address will trigger hugetlb_fault() in kernel,
	 * it will allocate and clear the huge page.*/
	for (i = 0; i < len/pagesz; i++) {
		*(volatile char *)addr = *addr;
		addr += pagesz;
	}

	return 0;
}

static int mmap_hugetlbfs(struct vmctx *ctx, size_t offset,
		void (*get_param)(struct hugetlb_info *, size_t *, size_t *),
		size_t (*adj_param)(struct hugetlb_info *, struct hugetlb_info *, int), char **addr)
{
	size_t len, skip;
	int level, ret = 0, pg_size;

	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		get_param(&hugetlb_priv[level], &len, &skip);
		pg_size = hugetlb_priv[level].pg_size;

		while (len > 0) {
			ret = mmap_hugetlbfs_from_level(ctx, level, len, offset, skip, addr);

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

#define FB_SIZE (16 * MB)
static void get_fbmem_param(struct hugetlb_info *htlb,
		size_t *len, size_t *skip)
{
	if (htlb == &hugetlb_priv[0]) {
		*len = FB_SIZE;
		*skip = htlb->lowmem + htlb->highmem + htlb->biosmem;
	} else {
		*len = 0;
		*skip = htlb->lowmem + htlb->highmem + htlb->biosmem;
	}
}

static size_t adj_fbmem_param(struct hugetlb_info *htlb,
		struct hugetlb_info *htlb_prev, int adj_size)
{
	htlb->fbmem -= adj_size;
	htlb_prev->fbmem += adj_size;

	return htlb->fbmem;
}

static int read_sys_info(const char *sys_path)
{
	FILE *fp;
	char tmp_buf[12];
	int pages = 0;
	int result;

	fp = fopen(sys_path, "r");
	if (fp == NULL) {
		pr_err("can't open: %s, err: %s\n", sys_path, strerror(errno));
		return 0;
	}

	memset(tmp_buf, 0, 12);
	result = fread(&tmp_buf, sizeof(char), 8, fp);
	if (result <= 0)
		pr_err("read %s, error: %s, please check!\n",
			sys_path, strerror(errno));
	else
		pages = strtol(tmp_buf, NULL, 10);

	fclose(fp);
	return pages;
}

/* check if enough free huge pages for the User VM */
static bool hugetlb_check_memgap(void)
{
	int lvl, free_pages, need_pages;
	bool has_gap = false;

	for (lvl = HUGETLB_LV1; lvl < hugetlb_lv_max; lvl++) {
		if (hugetlb_priv[lvl].fd < 0) {
			hugetlb_priv[lvl].pages_delta = 0;
			continue;
		}
		free_pages = read_sys_info(hugetlb_priv[lvl].free_pages_path);
		need_pages = (hugetlb_priv[lvl].lowmem + hugetlb_priv[lvl].fbmem +
			hugetlb_priv[lvl].biosmem + hugetlb_priv[lvl].highmem) /
			hugetlb_priv[lvl].pg_size;

		hugetlb_priv[lvl].pages_delta = need_pages - free_pages;
		/* if delta > 0, it's a gap for needed pages, to be handled */
		if (hugetlb_priv[lvl].pages_delta > 0)
			has_gap = true;

		pr_info("level %d free/need pages:%d/%d page size:0x%x\n", lvl,
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
		pr_err("cmd: %s failed!\n", cmd_buf);
		return;
	}
	pclose(fp);

	pr_info("to reserve pages (+orig %d): %s\n", orig_pages, cmd_buf);
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

		/* free one un-used larger page */
		orig_pages = read_sys_info(hugetlb_priv[level].nr_pages_path);
		total_pages = orig_pages - 1;
		snprintf(cmd_buf, MAX_PATH_LEN, "echo %d > %s",
			total_pages, hugetlb_priv[level].nr_pages_path);

		fp = popen(cmd_buf, "r");
		if (fp == NULL) {
			pr_err("cmd to free mem: %s failed!\n", cmd_buf);
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
 *.   even enough free memory, it is easier to reserve smaller pages than
 * lager ones, for example:2MB easier than 1GB. One flow of current solution:
 *.it could leave Service VM very small free memory.
 *.return value: true: success; false: failure
 */
static bool hugetlb_reserve_pages(void)
{
	int left_gap = 0, pg_size;
	int level;

	pr_info("to reserve more free pages:\n");
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
		 * free memory, it still can fail to allocate 1GB huge page.
		 * so if that, it needs the next level to handle it.
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
		pr_err("level %d pages gap: %d failed to reserve!\n",
			level, left_gap);
		return false;
	}

	pr_info("now enough free pages are reserved!\n");
	return true;
}

bool init_hugetlb(void)
{
	char path[MAX_PATH_LEN] = {0};
	int i;
	int level;
	int fd;
	size_t len;

	/* Try to create the dir of /run/hugetlb/acrn */
	snprintf(path, MAX_PATH_LEN, "%s/", ACRN_HUGETLB_LOCK_DIR);
	len = strnlen(path, MAX_PATH_LEN);
	for (i = 1; i < len; i++) {
		if (path[i] == '/') {
			path[i] = 0;
			if (access(path, F_OK) != 0) {
				if (mkdir(path, 0755) < 0) {
					/* We might have multiple acrn-dm instances booting VMs at
					 * the same time
					 */
					if (errno != EEXIST) {
						pr_err("mkdir %s failed: %s\n", path, errormsg(errno));
						return false;
					}
				}
			}
			path[i] = '/';
		}
	}

	for (level = HUGETLB_LV1; level < HUGETLB_LV_MAX; level++) {
		hugetlb_priv[level].fd = -1;
		fd = memfd_create("acrn_memfd", hugetlb_priv[level].flags);
		if (fd == -1)
			break;

		hugetlb_priv[level].fd = fd;
	}

	if (level == HUGETLB_LV1) /* mount fail for level 1 */
		return false;
	else if (level == HUGETLB_LV2) /* mount fail for level 2 */
		pr_warn("WARNING: only level 1 hugetlb supported");

	lock_fd = open(ACRN_HUGETLB_LOCK_FILE, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (lock_fd < 0) {
		return false;
	}
	lseek(lock_fd, SEEK_SET, LOCK_OFFSET_START);

	hugetlb_lv_max = level;

	return true;
}

void uninit_hugetlb(void)
{
	int level;
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		if (hugetlb_priv[level].fd > 0)
			close(hugetlb_priv[level].fd);
		hugetlb_priv[level].fd = -1;
	}

	close(lock_fd);
}

int hugetlb_setup_memory(struct vmctx *ctx)
{
	int level;
	size_t lowmem, fbmem, biosmem, highmem;
	bool has_gap;
	int fd;
	unsigned int seal_flag = F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
	size_t mem_size_level;

	mem_idx = 0;
	memset(&mmap_mem_regions, 0, sizeof(mmap_mem_regions));
	if (ctx->lowmem == 0) {
		pr_err("vm requests 0 memory");
		goto err;
	}

	/* In course of reboot sequence, the memfd is closed. So it
	 * needs to recreate the memfd if it is closed.
	 */
	for (level = HUGETLB_LV1; level < HUGETLB_LV_MAX; level++) {
		if (hugetlb_priv[level].fd > 0)
			continue;

		fd = memfd_create("acrn_memfd", hugetlb_priv[level].flags);
		if (fd == -1) {
			pr_err("Fail to create memfd for %d.\n",
				level);
			break;
		}
		hugetlb_priv[level].fd = fd;
	}

	if (ALIGN_CHECK(ctx->lowmem, hugetlb_priv[HUGETLB_LV1].pg_size) ||
		ALIGN_CHECK(ctx->highmem, hugetlb_priv[HUGETLB_LV1].pg_size) ||
		ALIGN_CHECK(ctx->biosmem, hugetlb_priv[HUGETLB_LV1].pg_size) ||
		ALIGN_CHECK(ctx->fbmem, hugetlb_priv[HUGETLB_LV1].pg_size)) {
		pr_err("Memory size is not aligned to 2M.\n");
		goto err;
	}
	/* all memory should be at least aligned with
	 * hugetlb_priv[HUGETLB_LV1].pg_size */
	ctx->lowmem =
		ALIGN_DOWN(ctx->lowmem, hugetlb_priv[HUGETLB_LV1].pg_size);
	ctx->fbmem =
		ALIGN_DOWN(ctx->fbmem, hugetlb_priv[HUGETLB_LV1].pg_size);
	ctx->biosmem =
		ALIGN_DOWN(ctx->biosmem, hugetlb_priv[HUGETLB_LV1].pg_size);
	ctx->highmem =
		ALIGN_DOWN(ctx->highmem, hugetlb_priv[HUGETLB_LV1].pg_size);

	total_size = ctx->highmem_gpa_base + ctx->highmem;

	/* check & set hugetlb level memory size for lowmem/biosmem/highmem */
	lowmem = ctx->lowmem;
	fbmem = ctx->fbmem;
	biosmem = ctx->biosmem;
	highmem = ctx->highmem;

	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		if (hugetlb_priv[level].fd < 0) {
			hugetlb_priv[level].lowmem = 0;
			hugetlb_priv[level].highmem = 0;
			hugetlb_priv[level].biosmem = 0;
			hugetlb_priv[level].fbmem = 0;
			continue;
		}
		hugetlb_priv[level].lowmem =
			ALIGN_DOWN(lowmem, hugetlb_priv[level].pg_size);
		hugetlb_priv[level].fbmem =
			ALIGN_DOWN(fbmem, hugetlb_priv[level].pg_size);
		hugetlb_priv[level].biosmem =
			ALIGN_DOWN(biosmem, hugetlb_priv[level].pg_size);
		hugetlb_priv[level].highmem =
			ALIGN_DOWN(highmem, hugetlb_priv[level].pg_size);

		if (level > HUGETLB_LV1) {
			hugetlb_priv[level-1].lowmem = lowmem =
				lowmem - hugetlb_priv[level].lowmem;
			hugetlb_priv[level-1].fbmem = fbmem =
				fbmem - hugetlb_priv[level].fbmem;
			hugetlb_priv[level-1].biosmem = biosmem =
				biosmem - hugetlb_priv[level].biosmem;
			hugetlb_priv[level-1].highmem = highmem =
				highmem - hugetlb_priv[level].highmem;
		}
	}

	lock_acrn_hugetlb();

	/* it will check each level memory need */
	has_gap = hugetlb_check_memgap();
	if (has_gap) {
		if (!hugetlb_reserve_pages())
			goto err_lock;
	}

	/* align up total size with huge page size for vma alignment */
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		if (should_enable_hugetlb_level(level)) {
			total_size += hugetlb_priv[level].pg_size;
			break;
		}
	}

	/* dump hugepage trying to setup */
	pr_info("\ntry to setup hugepage with:\n");
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		pr_info("\tlevel %d - lowmem 0x%lx, fbmem 0x%lx, biosmem 0x%lx, highmem 0x%lx\n",
			level,
			hugetlb_priv[level].lowmem,
			hugetlb_priv[level].fbmem,
			hugetlb_priv[level].biosmem,
			hugetlb_priv[level].highmem);
	}
	pr_info("total_size 0x%lx\n\n", total_size);

	/* basic overview vma */
	ptr = mmap(NULL, total_size, PROT_NONE,
			MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		pr_err("anony mmap fail");
		goto err_lock;
	}

	/* align up baseaddr according to hugepage level size */
	for (level = hugetlb_lv_max - 1; level >= HUGETLB_LV1; level--) {
		if (should_enable_hugetlb_level(level)) {
			ctx->baseaddr = (void *)ALIGN_UP((size_t)ptr,
						hugetlb_priv[level].pg_size);
			break;
		}
	}
	pr_info("mmap ptr 0x%p -> baseaddr 0x%p\n", ptr, ctx->baseaddr);

	/* mmap lowmem */
	if (mmap_hugetlbfs(ctx, 0, get_lowmem_param, adj_lowmem_param, NULL) < 0) {
		pr_err("lowmem mmap failed");
		goto err_lock;
	}

	/* mmap highmem */
	if (mmap_hugetlbfs(ctx, ctx->highmem_gpa_base,
				get_highmem_param, adj_highmem_param, NULL) < 0) {
		pr_err("highmem mmap failed");
		goto err_lock;
	}

	/* mmap biosmem */
	if (mmap_hugetlbfs(ctx, 4 * GB - ctx->biosmem,
				get_biosmem_param, adj_biosmem_param, NULL) < 0) {
		pr_err("biosmem mmap failed");
		goto err_lock;
	}

	/* mmap fbmem */
	if (mmap_hugetlbfs(ctx, 4 * GB - ctx->biosmem - ctx->fbmem,
		get_fbmem_param, adj_fbmem_param, (char **)&ctx->fb_base) < 0) {
		pr_err("fbmem mmap failed");
		goto err_lock;
	}

	/* resize the memfd to meet with the size requirement and add the
	 * F_SEAL_SEAL flag
	 */
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		if (hugetlb_priv[level].fd > 0) {
			mem_size_level = hugetlb_priv[level].lowmem +
					 hugetlb_priv[level].highmem +
					 hugetlb_priv[level].biosmem +
					 hugetlb_priv[level].fbmem;
			if (ftruncate(hugetlb_priv[level].fd, mem_size_level) == -1) {
				pr_err("Fail to set mem_size for level %d.\n",
					level);
				goto err_lock;
			}

			if (fcntl(hugetlb_priv[level].fd, F_ADD_SEALS,
				seal_flag) == -1) {
				pr_err("Fail to set seal flag for level %d.\n",
					level);
				goto err_lock;
			}
		}
	}


	unlock_acrn_hugetlb();

	/* dump hugepage really setup */
	pr_info("\nreally setup hugepage with:\n");
	for (level = HUGETLB_LV1; level < hugetlb_lv_max; level++) {
		pr_info("\tlevel %d - lowmem 0x%lx, biosmem 0x%lx, highmem 0x%lx\n",
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
		/*
		 * The High BIOS region can behave as RAM and be
		 * modified by the boot firmware itself (e.g. OVMF
		 * NV data storage region).
		 */
		if (vm_map_memseg_vma(ctx, ctx->biosmem, 4 * GB - ctx->biosmem,
			(uint64_t)(ctx->baseaddr + 4 * GB - ctx->biosmem),
			PROT_ALL) < 0)
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

err_lock:
	unlock_acrn_hugetlb();
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

bool
vm_find_memfd_region(struct vmctx *ctx, vm_paddr_t gpa,
			struct vm_mem_region *ret_region)
{
	int i;
	uint64_t offset;
	struct vm_mmap_mem_region *mmap_region;
	bool ret;

	mmap_region = NULL;
	for (i = 0; i < mem_idx; i++) {
		if ((gpa >= mmap_mem_regions[i].gpa_start) &&
			(gpa < mmap_mem_regions[i].gpa_end)) {
			mmap_region = &mmap_mem_regions[i];
			break;
		}
	}
	if (mmap_region && ret_region) {
		ret = true;
		offset = gpa - mmap_region->gpa_start;
		ret_region->fd = mmap_region->fd;
		ret_region->fd_offset = offset + mmap_region->fd_offset;
	} else
		ret = false;

	return ret;
}

bool vm_allow_dmabuf(struct vmctx *ctx)
{
	uint32_t mem_flags;

	mem_flags = 0;
	if (ctx->highmem) {
		/* Check the highmem is used by HUGETLB_LV1/HUGETLB_LV2 */
		if ((hugetlb_priv[HUGETLB_LV1].fd > 0) &&
			(hugetlb_priv[HUGETLB_LV1].highmem))
			mem_flags |= 1;
		if ((hugetlb_priv[HUGETLB_LV2].fd > 0) &&
			(hugetlb_priv[HUGETLB_LV2].highmem))
			mem_flags |= 0x02;
		if (mem_flags == 0x03)
			return false;
	}

	if (ctx->lowmem) {
		/* Check the lowmem is used by HUGETLB_LV1/HUGETLB_LV2 */
		mem_flags = 0;
		if ((hugetlb_priv[HUGETLB_LV1].fd > 0) &&
			(hugetlb_priv[HUGETLB_LV1].lowmem))
			mem_flags |= 1;
		if ((hugetlb_priv[HUGETLB_LV2].fd > 0) &&
			(hugetlb_priv[HUGETLB_LV2].lowmem))
			mem_flags |= 0x02;
		if (mem_flags == 0x03)
			return false;
	}
	return true;
}
