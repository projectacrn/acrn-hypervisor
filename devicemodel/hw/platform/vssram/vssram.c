/*
 * Copyright (C) 2021-2022 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/ioctl.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>

#include "pci_core.h"
#include "vmmapi.h"
#include "acpi.h"
#include "dm_string.h"
#include "log.h"
#include "vssram.h"
#include "tcc_buffer.h"

#define RTCT_V1 1
#define RTCT_V2 2

#define RTCT_ENTRY_HEADER_SIZE 8
#define foreach_rtct_entry(rtct, e) \
	for (e = (void *)rtct + sizeof(struct acpi_table_hdr); \
		((uint64_t)e - (uint64_t)rtct) < rtct->length; \
		e = (struct rtct_entry *)((uint64_t)e + e->size))

#define L2_CACHE 2
#define L3_CACHE 3
#define INVALID_CACHE_ID (-1)
#define INVALID_FD (-1)
#define VSSRAM_VCPUMASK_ALL ((uint64_t)-1)
#define MAX_VSSRAM_BUFFER_NUM (ACRN_PLATFORM_LAPIC_IDS_MAX << 1)

struct vssram_buf_param {
	int level;
	uint32_t size;
	uint64_t vcpumask;
};

struct vssram_buf {
	int fd;
	int level;
	int cache_id;
	uint32_t size;
	uint32_t l3_inclusive_of_l2_size;
	uint64_t vma_base;
	uint64_t gpa_base;
	uint32_t waymask;
	cpu_set_t pcpumask;
};

struct tcc_memory_info {
	int region_cnt;
	cpu_set_t *cpuset;
	struct tcc_mem_region_config *region_configs;
};

static uint16_t guest_vcpu_num;
static uint64_t guest_pcpumask;
static uint32_t guest_l2_cat_shift;
static uint32_t guest_l3_cat_shift;
static int      is_l3_inclusive_of_l2;
static uint32_t guest_lapicid_tbl[ACRN_PLATFORM_LAPIC_IDS_MAX];

static uint64_t vssram_size;
static uint64_t vssram_gpa_base;
static struct acpi_table_hdr  *vrtct_table;

static int	tcc_buffer_fd = INVALID_FD;
static struct	vssram_buf *vssram_buffers;
static struct   vssram_buf_param *vssram_buf_params;

#define vcpuid2pcpuid(vcpuid) pcpuid_from_vcpuid(guest_pcpumask, vcpuid)

static inline uint64_t vcpuid2lapicid(int vcpuid)
{
	return guest_lapicid_tbl[vcpuid];
}

static inline int lapicid2cacheid(uint64_t lapicid, int level)
{
	return ((level == L2_CACHE) ? (lapicid >> guest_l2_cat_shift) :
		(lapicid >> guest_l3_cat_shift));
}

static inline int vcpuid2cacheid(int vcpuid, int level)
{
	return  lapicid2cacheid(vcpuid2lapicid(vcpuid), level);
}

static uint8_t vrtct_checksum(uint8_t *vrtct, uint32_t length)
{
	uint8_t sum = 0;
	uint32_t i;

	for (i = 0; i < length; i++) {
		sum += vrtct[i];
	}
	return -sum;
}

static inline struct rtct_entry *get_free_rtct_entry(struct acpi_table_hdr *rtct)
{
	return (struct rtct_entry *)((uint8_t *)rtct + rtct->length);
}

static inline void add_rtct_entry(struct acpi_table_hdr *rtct, struct rtct_entry *e)
{
	rtct->length += e->size;
}

/**
 * @brief  Add a native entry to virtual RTCT.
 *
 * @param vrtct  Pointer to virtual RTCT.
 * @param entry  Pointer to native RTCT entry.
 *
 * @return 0 on success and non-zero on fail.
 */
int vrtct_add_native_entry(struct acpi_table_hdr *vrtct, struct rtct_entry *entry)
{
	struct rtct_entry *rtct_entry;

	rtct_entry = get_free_rtct_entry(vrtct);
	memcpy((void *)rtct_entry, (void *)entry, entry->size);

	add_rtct_entry(vrtct, rtct_entry);
	return 0;
}

/**
 * @brief  Add a software SRAM cache way mask entry
 *         associated with software SRAM to virtual RTCT.
 *
 * @param vrtct     Pointer to virtual RTCT.
 * @param cache_id  Cache ID.
 * @param level     Cache level.
 * @param waymask   The cache ways bitmap.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vrtct_add_ssram_waymask(struct acpi_table_hdr *vrtct, int cache_id,
			int level, uint32_t waymask)
{
	struct rtct_entry *entry;
	struct rtct_entry_data_ssram_waymask *ssram_waymask;

	entry = get_free_rtct_entry(vrtct);
	entry->format_version = 1;
	entry->type = RTCT_V2_SSRAM_WAYMASK;

	ssram_waymask = (struct rtct_entry_data_ssram_waymask *)entry->data;
	ssram_waymask->cache_level = level;
	ssram_waymask->cache_id = cache_id;
	ssram_waymask->waymask = waymask;

	entry->size = RTCT_ENTRY_HEADER_SIZE + sizeof(*ssram_waymask);
	add_rtct_entry(vrtct, entry);

	return 0;
}

/**
 * @brief  Add a software SRAM entry associated with
 *         software SRAM to virtual RTCT table.
 *
 * @param vrtct     Pointer to virtual RTCT.
 * @param cache_id  Cache ID.
 * @param level     Cache level.
 * @param base      Base address (GPA) to this software SRAM region.
 * @param size      Size of this software SRAM region.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vrtct_add_ssram_v2_entry(struct acpi_table_hdr *vrtct, int cache_id,
			int level, uint64_t base, size_t size)
{
	struct rtct_entry *entry;
	struct rtct_entry_data_ssram_v2 *ssram_v2;

	entry = get_free_rtct_entry(vrtct);
	entry->format_version = 2;
	entry->type = RTCT_V2_SSRAM;

	ssram_v2 = (struct rtct_entry_data_ssram_v2 *)entry->data;
	ssram_v2->cache_level = level;
	ssram_v2->cache_id = cache_id;
	ssram_v2->base = base;
	ssram_v2->size = size;
	ssram_v2->shared = 0;

	entry->size = RTCT_ENTRY_HEADER_SIZE + sizeof(*ssram_v2);
	add_rtct_entry(vrtct, entry);

	return 0;
}

/**
 * @brief  Initialize file descriptor of TCC buffer driver interface.
 *
 * @param void.
 *
 * @return 0 on success and non-zero on fail.
 *
 * @note Global variable 'tcc_buffer_fd' is initialized.
 */
static int tcc_driver_init_buffer_fd(void)
{
	int fd;

	if (tcc_buffer_fd != INVALID_FD)
		return tcc_buffer_fd;

	fd = open(TCC_BUFFER_DEVNODE, O_RDWR);
	if (fd == INVALID_FD) {
		pr_err("%s failed: %s: %s(%i)", __func__,
			TCC_BUFFER_DEVNODE, strerror(errno), errno);
		return INVALID_FD;
	}
	tcc_buffer_fd = fd;

	return 0;
}

/**
 * @brief  Disable CPU affinity check in TCC driver.
 *
 * @param void.
 *
 * @return 0 on success and -1 on fail.
 */
static int disable_tcc_strict_affinity_check(void)
{
#define AFFINITY_CHECK_PARAM "/sys/module/tcc_buffer/parameters/strict_affinity_check"
	int fd, ret = 0U;

	fd = open(AFFINITY_CHECK_PARAM, O_WRONLY);
	if (fd < 0) {
		pr_err("%s: Failed to open %s: %s(%i), please check its availability.\n",
			__func__, AFFINITY_CHECK_PARAM, strerror(errno), errno);
		return -1;
	}

	/* Value of 0 means turning off CPU affinity check. */
	if (write(fd, "0", 1) < 0) {
		pr_err("%s: Failed to turn off affinity checking in the TCC driver"
			"(could not write to: %s: %s(%i)", __func__,
			AFFINITY_CHECK_PARAM, strerror(errno), errno);
		ret = -1;
	}

	close(fd);
	return ret;
}


/**
 * @brief  Get count of TCC software SRAM regions.
 *
 * @param void.
 *
 * @return Count of software SRAM regions on success
 *         and -1 on fail.
 */
static int tcc_driver_get_region_count(void)
{
	int buffer_count = 0;

	if (ioctl(tcc_buffer_fd, TCC_GET_REGION_COUNT, &buffer_count) < 0) {
		pr_err("%s failed: fd=%d: %s(%i)\n",
			__func__, tcc_buffer_fd, strerror(errno), errno);
		return -1;
	}

	return buffer_count;
}

/**
 * @brief  Get configurations of given TCC software SRAM region.
 *
 * @param region_id Target software region index.
 * @param config    Pointer to buffer to be filled with structure
 *                  tcc_buf_mem_config_s instance.
 *
 * @return 0 on success and non-zero on fail.
 */
static int tcc_driver_get_memory_config(int region_id, struct tcc_mem_region_config *config)
{
	config->id = region_id;
	CPU_ZERO((cpu_set_t *)config->cpu_mask_p);
	if ((ioctl(tcc_buffer_fd, TCC_GET_MEMORY_CONFIG, config)) < 0) {
		pr_err("%s failed: fd=%d, region_id=%u, error: %s(%i)\n",
			__func__, tcc_buffer_fd, region_id, strerror(errno), errno);
		return -1;
	}
	return 0;
}

/**
 * @brief  Request TCC software SRAM buffer from region specified by region ID.
 *
 * @param region_id  Target software SRAM region ID.
 * @param size       Request buffer size in bytes.
 *
 * @return device node ID on success and -1 on fail.
 */
static int tcc_driver_req_buffer(unsigned int region_id, unsigned int size)
{
	struct tcc_buf_mem_req req;

	memset(&req, 0, sizeof(req));
	req.devnode_id = -1;
	req.id = region_id;
	req.size = size;

	if (ioctl(tcc_buffer_fd, TCC_REQ_BUFFER, &req) < 0) {
		pr_err("%s failed: fd=%d, region_id=%u, size=%ld KB.\n",
			__func__, tcc_buffer_fd, req.id, req.size);
		return -1;
	}

	pr_info("%s succ: region_id: %d, size: %ld KB, dev_node=%d.\n",
		__func__, region_id, req.size >> 10, req.devnode_id);

	return (int)req.devnode_id;
}

/**
 * @brief  Read native RTCT table from TCC buffer driver.
 *
 * @param void
 *
 * @return Poiner to RTCT data buffer on success and NULL on fail.
 *
 * @note The returned pointer must be passed to free to avoid a memory leak.
 */
static void *tcc_driver_read_rtct(void)
{
	void *buffer;

	buffer = calloc(1, 4096);
	if (buffer == NULL) {
		pr_err("%s, calloc failed.\n", __func__);
		return NULL;
	}

	if (ioctl(tcc_buffer_fd, TCC_GET_RTCT, (unsigned int *)buffer) < 0) {
		free(buffer);
		return NULL;
	}
	return buffer;
}

/**
 * @brief  Get file descriptor handler to TCC cache buffer.
 *
 * @param devnode_id  Device node ID of TCC buffer.
 *
 * @return         File descriptor on success and -1 on fail.
 */
static int vssram_open_buffer(int devnode_id)
{
	int fd;
	char buffer_file_name[256];

	sprintf(buffer_file_name, "%s%i", TCC_BUFFER_DEVNODE, devnode_id);

	fd = open(buffer_file_name, O_RDWR);
	if (fd == INVALID_FD) {
		pr_err("%s, Failure open('%s', O_RDRW):error: %s(%i)",
			__func__, buffer_file_name, strerror(errno), errno);
	}

	return fd;
}

/**
 * @brief Release all vSSRAM buffers and close file
 *        descriptors of TCC cache buffers.
 *
 * @param void
 *
 * @return void
 */
static void vssram_close_buffers(void)
{
	int i;
	struct vssram_buf *vbuf;

	if (!vssram_buffers)
		return;

	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf = &vssram_buffers[i];
		if (vbuf->fd != -1) {
			munmap((void *)vbuf->vma_base, vbuf->size);
			close(vbuf->fd);
		}
	}

	if (tcc_buffer_fd != INVALID_FD) {
		close(tcc_buffer_fd);
		tcc_buffer_fd = INVALID_FD;
	}
}

/**
 * @brief        Creates a memory mapping for cache buffer.
 *
 * @param fd     File descriptor of cache buffer device node.
 * @param size   Size of cache buffer
 *
 * @return       Pointer to the mapped area on success and NULL on fail.
 */
static void *vssram_mmap_buffer(int fd, size_t size)
{
	void *memory;

	memory = mmap(NULL, size, 0, MAP_FILE | MAP_SHARED, fd, 0);
	if (memory == MAP_FAILED) {
		pr_err("%s, Failure mmap on fd:%d, size:%ld.\n", __func__, fd, size);
		return NULL;
	}

	return memory;
}

/**
 * @brief  Setup EPT mapping of a software SRAM buffer for user VM.
 *
 * @param ctx       Pointer to context of user VM.
 * @param buf_desc  Pointer to software SRAM buffer description.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vssram_ept_map_buffer(struct vmctx *ctx, struct vssram_buf *buf_desc)
{
	struct acrn_vm_memmap memmap = {
		.type = ACRN_MEMMAP_RAM,
		.vma_base = 0,
		.len = 0,
		.attr = ACRN_MEM_ACCESS_RWX
	};
	int error;
	memmap.vma_base = buf_desc->vma_base;
	memmap.user_vm_pa = buf_desc->gpa_base;
	memmap.len = buf_desc->size;
	error = ioctl(ctx->fd, ACRN_IOCTL_UNSET_MEMSEG, &memmap);
	if (error) {
		pr_err("ACRN_IOCTL_UNSET_MEMSEG ioctl() returned an error: %s\n", errormsg(errno));
	}
	error = ioctl(ctx->fd, ACRN_IOCTL_SET_MEMSEG, &memmap);
	if (error) {
		pr_err("ACRN_IOCTL_SET_MEMSEG ioctl() returned an error: %s\n", errormsg(errno));
	}
	return error;
};

static int init_guest_lapicid_tbl(uint64_t guest_pcpu_bitmask)
{
	int pcpu_id = 0, vcpu_id = 0;
	int vcpu_num = bitmap_weight(guest_pcpu_bitmask);

	for (vcpu_id = 0; vcpu_id < vcpu_num; vcpu_id++) {
		pcpu_id = pcpuid_from_vcpuid(guest_pcpu_bitmask, vcpu_id);
		if (pcpu_id < 0)
			return -1;

		guest_lapicid_tbl[vcpu_id] = lapicid_from_pcpuid(pcpu_id);
	}
	return 0;
}

/**
 * @brief  Get basic cache hierarchy information, including the
 *          order value of cache threads sharing number and
 *          the inclusiveness of LLC.
 *
 * @param l2_shift: Poiner to a buffer to be filled with the
 *                  order value of L2 cache threads sharing number.
 * @param l3_shift: Poiner to a buffer to be filled with the
 *                  order value of L3 cache threads sharing number.
 *
 * @return Inclusiveness status of LLC(Last Level Cache),
 *         1: LLC is inclusive, 0: LLC is not inclusive.
 *
 */
static int get_cache_hierarchy_info(uint32_t *l2_shift, uint32_t *l3_shift)
{
	int llc_inclusive = 0;
	uint32_t subleaf, eax, ebx, ecx, edx;
	uint32_t cache_type, cache_level, id, shift;

	*l2_shift = 0;
	*l3_shift = 0;

	/* The 0x04 leaf of cpuid is pass-throughed to service VM. */
	for (subleaf = 0;; subleaf++) {
		do_cpuid(0x4, subleaf, &eax, &ebx, &ecx, &edx);

		cache_type = eax & 0x1f;
		cache_level = (eax >> 5) & 0x7;
		id = (eax >> 14) & 0xfff;
		shift = get_num_order(id + 1);

		/* No more caches */
		if ((cache_type == 0) || (cache_type >= 4)) {
			break;
		}

		if (cache_level == 2) {
			*l2_shift = shift;
		} else if (cache_level == 3) {
			*l3_shift = shift;
			/* EDX: Bit 01: Cache Inclusiveness.
			 * 0 = Cache is not inclusive of lower cache levels.
			 * 1 = Cache is inclusive of lower cache levels.
			 */
			llc_inclusive = (edx >> 1) & 0x1;
		}
	}
	return llc_inclusive;
}
/**
 * @brief  Get the L3 cache ID for given L2 vSSRAM buffer
 *
 * @param l2_vbuf Pointer to a L2 vSSRAM buffer descripition.
 *
 * @return L3 cache ID.
 */
static int get_l3_cache_id_from_l2_buffer(struct vssram_buf *l2_vbuf)
{
	assert(guest_l3_cat_shift >= guest_l2_cat_shift);
	assert(l2_vbuf->level == L2_CACHE);

	return (l2_vbuf->cache_id >> (guest_l3_cat_shift - guest_l2_cat_shift));
}

/**
 * @brief  Parse a vSSRAM buffer parameter and add it
 *         to vSSRAM buffer descripition table.
 *
 * @param vbuf_tbl Pointer to description table of cache buffers.
 * @param vbuf_param    Pointer to a cache buffer configuration from user input.
 *
 * @return 0 on success and -1 on fail.
 */
static int vssram_add_buffer(struct vssram_buf *vbuf_tbl, struct vssram_buf_param *vbuf_param)
{
	int i, pcpuid, cacheid = INVALID_CACHE_ID;
	uint64_t vcpumask = vbuf_param->vcpumask;
	struct vssram_buf *vbuf;

	/* all vCPUs are configured for this vSSRAM region */
	if (vcpumask == VSSRAM_VCPUMASK_ALL) {
		vcpumask = 0;
		for (i = 0; i < guest_vcpu_num; i++)
			vcpumask |= (1 << i);
	}

	/*
	 * sanity check on vSSRAM region configuration against guest cache hierarchy,
	 * all vCPUs configured in this region shall share the same level specific cache.
	 */
	for (i = 0; i < ACRN_PLATFORM_LAPIC_IDS_MAX; i++) {
		if ((vcpumask >> i) & 0x1) {
			if (cacheid == INVALID_CACHE_ID) {
				cacheid = vcpuid2cacheid(i, vbuf_param->level);
			} else if (cacheid != vcpuid2cacheid(i, vbuf_param->level)) {
				pr_err("The vSSRAM param and cache hierarchy are mismatched.\n");
				return -1;
			}
		}
	}
	assert(cacheid != INVALID_CACHE_ID);

	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf = &vbuf_tbl[i];
		if (vbuf->size == 0) {
			/* fill a new vSSRAM buffer */
			vbuf->level = vbuf_param->level;
			vbuf->cache_id = cacheid;
			vbuf->size = vbuf_param->size;
			break;
		}

		if ((vbuf->cache_id == cacheid) && (vbuf->level == vbuf_param->level)) {
			/* merge it to vSSRAM buffer */
			vbuf->size += vbuf_param->size;
			break;
		}
	}
	assert(i < MAX_VSSRAM_BUFFER_NUM);

	/* update pcpumask of vSSRAM buffer */
	for (i = 0; i < ACRN_PLATFORM_LAPIC_IDS_MAX; i++) {
		if ((vcpumask >> i) & 0x1) {
			pcpuid = vcpuid2pcpuid(i);
			if ((pcpuid < 0) || (pcpuid >= ACRN_PLATFORM_LAPIC_IDS_MAX)) {
				pr_err("%s, invalid pcpuid(%d) from vcpuid(%d).\n", __func__, pcpuid, i);
				return -1;
			}
			CPU_SET(pcpuid, &vbuf->pcpumask);
		}
	}
	return 0;
}

/**
 * @brief  Extend L3 vSSRAM buffer for L3 cache inclusive case when any L2 vSSRAM buffers
 *         are configured, new L3 vSSRAM buffer will be added when it is not found.
 *
 * @param vbuf_tbl Pointer vSSRAM buffer table.
 *
 * @return void
 */
static void vssram_extend_l3buf_for_l2buf(struct vssram_buf *vbuf_tbl)
{
	bool inclusive_l3_found;
	int i, j, l3_cache_id;
	struct vssram_buf *vbuf1, *vbuf2;

	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf1 = &vbuf_tbl[i];
		if (vbuf1->size == 0)
			break;

		if (vbuf1->level != L2_CACHE)
			continue;

		/*
		 * check if L3 vSSRAM buffer that is inclusive
		 * of this L2 buffer is configured.
		 */
		inclusive_l3_found = false;
		l3_cache_id = get_l3_cache_id_from_l2_buffer(vbuf1);
		for (j = 0; j < MAX_VSSRAM_BUFFER_NUM; j++) {
			if (j == i) /* skip itself */
				continue;

			vbuf2 = &vbuf_tbl[j];
			if (vbuf2->cache_id == INVALID_CACHE_ID)
				break;

			if ((vbuf2->level == L3_CACHE)
				&& (vbuf2->cache_id == l3_cache_id)) {
				inclusive_l3_found = true;
				break;
			}
		}

		if (j >= MAX_VSSRAM_BUFFER_NUM)
			return;

		/* 'vbuf2' points to a free vSSRAM buffer if 'inclusive_l3_found' is false,
		 *  else 'vbuf2' points to a configured L3 vSSRAM buffer.
		 *  'l3_inclusive_of_l2_size' of 'vbuf2' should be updated for either case.
		 */
		vbuf2->l3_inclusive_of_l2_size += vbuf1->size;

		if (inclusive_l3_found) {
			continue;
		} else {
			/* fill the new L3 cache buffer being inclusive of this L2 cache buffer */
			vbuf2->cache_id = l3_cache_id;
			vbuf2->level = L3_CACHE;
		}
	}
}

/**
 * @brief  Load configurations of all TCC software SRAM regions configured
 *         on current platform.
 *
 * @param config Pointer to configuration information of all sofware SRAM memory regions.
 *
 * @return 0 on success and -1 on fail.
 *
 * @note   The returned pointers 'config' and 'cpuset' must be passed to free
 *         to avoid a memory leak.
 */
static int load_tcc_memory_info(struct tcc_memory_info *mem_info)
{
	int i, rgn_cnt = -1;
	cpu_set_t *cpuset_p = NULL;
	struct tcc_mem_region_config *config_p = NULL;

	rgn_cnt = tcc_driver_get_region_count();
	if (rgn_cnt <= 0)
		return rgn_cnt;

	config_p = calloc(rgn_cnt, sizeof(struct tcc_mem_region_config));
	if (config_p == NULL)
		return -1;

	cpuset_p = calloc(rgn_cnt, sizeof(cpu_set_t));
	if (cpuset_p == NULL)
		goto err;

	for (i = 0; i < rgn_cnt; i++) {
		config_p[i].cpu_mask_p = (void *)(cpuset_p + i);
		if (tcc_driver_get_memory_config(i, config_p + i) < 0) {
			goto err;
		}
	}

	mem_info->region_cnt = rgn_cnt;
	mem_info->cpuset = cpuset_p;
	mem_info->region_configs = config_p;

	return 0;

err:
	if (config_p != NULL)
		free(config_p);

	if (cpuset_p != NULL)
		free(cpuset_p);

	return -1;
}

/**
 * @brief  Request cache buffer based on the TCC software SRAM regions configurations.
 *
 * @param vbuf     Pointer to cache buffer description.
 * @param mem_info Pointer to memory region configurations .
 *
 * @return device node of cache buffer on success and -1 on fail.
 */
static int vssram_request_buffer(struct vssram_buf *vbuf,
		struct tcc_memory_info *mem_info)
{
	int i, pcpuid = 0, devnode_id = -1;
	struct tcc_mem_region_config *config;

	if (CPU_COUNT(&vbuf->pcpumask) == 0) {
		pr_err("%s pCPU mask is not configured in cache buffer description.\n", __func__);
		return -1;
	}

	/* get pCPU ID that has affinity with current cache buffer, use 0 for L3 cache buffer,
	 * all CPUs in this pcpumask can access current cache buffer, so select anyone inside.
	 */
	for (pcpuid = 0; pcpuid < ACRN_PLATFORM_LAPIC_IDS_MAX; pcpuid++) {
		if (CPU_ISSET(pcpuid, &vbuf->pcpumask))
			break;
	}
	assert((pcpuid > 0) && (pcpuid < ACRN_PLATFORM_LAPIC_IDS_MAX));

	/* Request cache buffer from TCC kernel driver */
	for (i = 0; i < mem_info->region_cnt; i++) {
		config = mem_info->region_configs + i;
		if (CPU_ISSET(pcpuid, (cpu_set_t *)config->cpu_mask_p)
			&& (config->size >= vbuf->size)
			&& (config->type == vbuf->level)) {
			devnode_id = tcc_driver_req_buffer(i, vbuf->size);
			if (devnode_id > 0) {
				vbuf->waymask = config->ways;
				break;
			}
		}
	}
	return devnode_id;
}

/**
 * @brief  Allocate cache buffer from TCC software SRAM regions
 *         and setup memory mapping.
 *
 * @param vbuf     Pointer to cache buffer description.
 * @param mem_info Pointer to memory region configurations .
 *
 * @return 0 on success and -1 on fail.
 */
static int vssram_setup_buffer(struct vssram_buf *vbuf,
		struct tcc_memory_info *mem_info)
{
	void *ptr;
	int devnode_id, fd;

	devnode_id = vssram_request_buffer(vbuf, mem_info);
	if (devnode_id == -1) {
		pr_err("%s, request buffer failed, size:%x.\n",
			__func__, vbuf->size);
		return -1;
	}

	fd = vssram_open_buffer(devnode_id);
	if (fd == INVALID_FD) {
		pr_err("%s, open buffer device node(%d) failed.\n", __func__, devnode_id);
		return -1;
	}

	ptr = vssram_mmap_buffer(fd, vbuf->size);
	if (ptr == NULL) {
		pr_err("%s, mmap buffer failed, devnode_id:%.\n", __func__, devnode_id);
		return -1;
	}

	vbuf->vma_base = (uint64_t)ptr;
	vbuf->fd = fd;
	return 0;
}

/**
 * @brief  Prepare cache buffers according the cache buffer descriptions.
 *
 * @param mem_info Pointer to memory region configurations.
 *
 * @return 0 on success and -1 on fail.
 *
 * @pre vssram_buffers != NULL
 */
static int vssram_prepare_buffers(struct tcc_memory_info *mem_info)
{
	int i;
	struct vssram_buf *vbuf;

	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf = &vssram_buffers[i];

		if (vbuf->size == 0)
			break;

		if (vssram_setup_buffer(vbuf, mem_info) < 0) {
			pr_err("%s, allocate cache(level:%d, size:%x) failed\n",
				__func__, vbuf->level, vbuf->size);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief  Set GPA for vSSRAM buffers for inclusive cache hierarchy case,
 *         GPAs of two kinds of vSSRAM buffers will be updated:
 *          - Given L3 vSSRAM buffer.
 *          - All L2 vSSRAM buffers that this given L3 vSSRAM buffer is inclusive of.
 *
 * @param l3_vbuf   Pointer to a L3 vSSRAM buffer descripition.
 * @param gpa_start Start GPA for this mapping.
 *
 * @return Size of GPA space consumed by this mapping.
 */
static uint64_t vssram_merge_l2l3_gpa_regions(struct vssram_buf *l3_vbuf, uint64_t gpa_start)
{
	int i;
	uint64_t gpa = gpa_start;
	struct vssram_buf *vbuf;

	assert(l3_vbuf->level == L3_CACHE);
	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf = &vssram_buffers[i];
		if ((vbuf->level == L2_CACHE)
			&& (get_l3_cache_id_from_l2_buffer(vbuf) == l3_vbuf->cache_id)) {
			vbuf->gpa_base = gpa;
			gpa += vbuf->size;
		}
	}
	l3_vbuf->gpa_base = gpa;

	return (l3_vbuf->size + l3_vbuf->l3_inclusive_of_l2_size);
}

/**
 * @brief  Assign GPA base to all vSSRAM buffers for both inclusive cache
 *         case and non-inclusive cache case.
 *
 * @param void
 *
 * @return void
 */
static void vssram_config_buffers_gpa(void)
{
	int i, level;
	uint64_t gpa, gpa_size;
	struct vssram_buf *vbuf;

	gpa = vssram_gpa_base;

	/*
	 * For the L3 inclusive of L2 case, the L2 vSSRAM buffer and
	 * its L3 companion buffer need to map to same gpa region.
	 */
	if (is_l3_inclusive_of_l2) {
		for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
			vbuf = &vssram_buffers[i];
			if (vbuf->level == L3_CACHE) {
				gpa_size = vssram_merge_l2l3_gpa_regions(vbuf, gpa);
				gpa += gpa_size;
			}
		}
	} else {
		/*
		 * non inclusive cache case, map L2 cache buffers first, and then L3 buffers,
		 * there is no GPA space overlap between L2 and L3 cache buffers.
		 */
		for (level = L2_CACHE; level <= L3_CACHE; level++) {
			for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
				vbuf = &vssram_buffers[i];
				if (vbuf->level == level) {
					vbuf->gpa_base = gpa;
					gpa += vbuf->size;
				}
			}
		}
	}
}

/**
 * @brief  Setup EPT mapping for all vSSRAM buffers.
 *
 * @param ctx   Pointer to context of user VM.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vssram_ept_map_buffers(struct vmctx *ctx)
{
	int i;
	struct vssram_buf *vbuf;

	/* confiure GPA for vSSRAM buffers */
	vssram_config_buffers_gpa();

	/* setup EPT mapping for L2 & L3 cache buffers */
	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf = &vssram_buffers[i];

		if (vbuf->fd == INVALID_FD)
			break;

		if (vssram_ept_map_buffer(ctx, vbuf) < 0) {
			pr_err("%s, setup EPT mapping for cache buffer failed.\n", __func__);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief  Initialize L2 & L3 vSSRAM buffer context table.
 *
 * @param void
 *
 * @return 0 on success and non-zero on fail.
 */
static int vssram_init_buffers(void)
{
	int i;
	struct vssram_buf *vbuf_tbl;
	struct vssram_buf_param *param;

	if ((vssram_buffers != NULL) || (vssram_buf_params == NULL)) {
		pr_err("descripitions table is already initialized or lack of ssram configurations.\n");
		return -1;
	}

	vbuf_tbl = calloc(MAX_VSSRAM_BUFFER_NUM, sizeof(struct vssram_buf));
	if (vbuf_tbl == NULL)
		return -1;

	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf_tbl[i].fd = INVALID_FD;
		vbuf_tbl[i].cache_id = INVALID_CACHE_ID;
	}

	is_l3_inclusive_of_l2 = get_cache_hierarchy_info(&guest_l2_cat_shift, &guest_l3_cat_shift);
	pr_info("%s, l2_cat_shift:%d, l3_cat_shift:%d, l3_inclusive:%d.\n", __func__,
		guest_l2_cat_shift, guest_l3_cat_shift, is_l3_inclusive_of_l2);

	/* parse vSSRAM params and fill vSSRAM buffer table */
	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		param = &vssram_buf_params[i];
		if (param->size > 0) {
			if (vssram_add_buffer(vbuf_tbl, param) < 0) {
				pr_err("%s, failed to add cache buffer.\n", __func__);
				goto err;
			}
		}
	}

	if (is_l3_inclusive_of_l2) {
		vssram_extend_l3buf_for_l2buf(vbuf_tbl);
	}

	vssram_buffers = vbuf_tbl;
	return 0;
err:
	free(vbuf_tbl);
	return -1;
}

/**
 * @brief  Initialize ACPI header information of RTCT.
 *
 * @param vrtct Pointer to virtual RTCT.
 *
 * @return void
 */
static void vrtct_init_acpi_header(struct acpi_table_hdr *vrtct)
{
	/* initialize ACPI RTCT header information. */
	strncpy(vrtct->signature, "RTCT", 4);
	vrtct->length = sizeof(struct acpi_table_hdr);
	vrtct->revision = 1;
	strncpy(vrtct->oem_id, "INTEL", 6);
	strncpy(vrtct->oem_table_id, "EDK2", 8);
	vrtct->oem_revision = 0x5;
	strncpy(vrtct->asl_compiler_id, "INTL", 4);
	vrtct->asl_compiler_revision = 0x100000d;
}

/**
 * @brief  Add RTCT compatibility entry to virtual RTCT,
 *	   compatibility entry provides the version of RTCT
 *         and the supported RTCD version, this entry is hardcoded
 *         to support RTCT V2 only.
 *
 * @param vrtct  Pointer to virtual RTCT.
 *
 * @return void
 */
static void vrtct_add_compat_entry(struct acpi_table_hdr *vrtct)
{
	struct rtct_entry *entry;
	struct rtct_entry_data_compatibility *compat;

	entry = get_free_rtct_entry(vrtct);
	entry->format_version = 1;
	entry->type = RTCT_V2_COMPATIBILITY;

	compat = (struct rtct_entry_data_compatibility *)entry->data;
	compat->rtct_ver_major = 2;
	compat->rtct_ver_minor = 0;
	compat->rtcd_ver_major = 0;
	compat->rtcd_ver_Minor = 0;

	entry->size = RTCT_ENTRY_HEADER_SIZE + sizeof(*compat);
	add_rtct_entry(vrtct, entry);
}

/**
 * @brief  Add both L2 and L3 vSSRAM buffers to RTCT software SRAM
 *         and way mask entries.
 *
 * @param vrtct  Pointer to virtual RTCT.
 *
 * @return void.
 */
static void vrtct_add_ssram_entries(struct acpi_table_hdr *vrtct)
{
	int i;
	uint64_t inclusive_size;
	struct vssram_buf *vbuf;

	for (i = 0; i < MAX_VSSRAM_BUFFER_NUM; i++) {
		vbuf = &vssram_buffers[i];

		if (vbuf->cache_id == INVALID_CACHE_ID)
			break;

		/* add SSRAM & WAYMASK entries of this buffer to vRTCT */
		vrtct_add_ssram_waymask(vrtct, vbuf->cache_id, vbuf->level, vbuf->waymask);

		inclusive_size = ((is_l3_inclusive_of_l2) && (vbuf->level == L3_CACHE)) ?
			vbuf->l3_inclusive_of_l2_size : 0;

		vrtct_add_ssram_v2_entry(vrtct, vbuf->cache_id, vbuf->level,
			vbuf->gpa_base - inclusive_size, vbuf->size + inclusive_size);
	}
}

/**
 * @brief  Add memory hierarchy latency entries to virtual RTCT,
 *         these entries describe the "worst case" access latency
 *         to a particular memory hierarchy.
 *
 * @param vrtct Pointer to virtual RTCT.
 *
 * @return 0 on success and non-zero on fail.
 *
 * @note   1) Pass-through TCC native memory hierarchy latency entries
 *            to ACRN guests.
 *         2) From the user VM point of view, the actual latency will
 *            be worse as the cost of virtualization.
 */
static int vrtct_add_memory_hierarchy_entries(struct acpi_table_hdr *vrtct)
{
	int rtct_ver = RTCT_V1;
	struct rtct_entry *entry;
	struct acpi_table_hdr *tcc_rtct;
	struct rtct_entry_data_compatibility *compat;

	tcc_rtct = (struct acpi_table_hdr *)tcc_driver_read_rtct();
	if (tcc_rtct == NULL) {
		pr_err("%s, Read TCC RTCT table failed.\n", __func__);
		return -1;
	}

	/* get TCC RTCT version */
	foreach_rtct_entry(tcc_rtct, entry) {
		if (entry->type == RTCT_V2_COMPATIBILITY) {
			compat =  (struct rtct_entry_data_compatibility *)entry->data;
			rtct_ver = compat->rtct_ver_major;
			break;
		}
	}

	/* support RTCT V2 only */
	if (rtct_ver != RTCT_V2) {
		pr_err("%s, Warning: Unsupported RTCT version.\n", __func__);
		free(tcc_rtct);
		return -1;
	}

	foreach_rtct_entry(tcc_rtct, entry) {
		if (entry->type == RTCT_V2_MEMORY_HIERARCHY_LATENCY) {
			vrtct_add_native_entry(vrtct, entry);
		}
	}

	free(tcc_rtct);
	return 0;
}

/**
 * @brief Initialize all virtual RTCT entries.
 *
 * @param vrtct Pointer to virtual RTCT.
 *
 * @return 0 on success and non-zero on fail.
 */
static struct acpi_table_hdr *create_vrtct(void)
{
#define RTCT_BUF_SIZE 4096
	struct acpi_table_hdr  *vrtct;

	vrtct = malloc(RTCT_BUF_SIZE);
	if (vrtct == NULL) {
		pr_err("%s, allocate vRTCT buffer failed.\n", __func__);
		return NULL;
	}
	vrtct_init_acpi_header(vrtct);

	/* add compatibility entry */
	vrtct_add_compat_entry(vrtct);

	/* add SSRAM entries */
	vrtct_add_ssram_entries(vrtct);

	/* add memory hierarchy entries */
	if (vrtct_add_memory_hierarchy_entries(vrtct) < 0) {
		pr_err("%s, add ssram latency failed.\n", __func__);
		goto err;
	}

	vrtct->checksum = vrtct_checksum((uint8_t *)vrtct, vrtct->length);
	pr_info("%s, rtct len:%d\n", __func__, vrtct->length);
	return vrtct;

err:
	free(vrtct);
	return NULL;
}

/**
 * @brief Initialize guest GPA maximum space of vSSRAM ,
 *        including GPA base and maximum size.
 *
 * @param void.
 *
 * @return void.
 */
static void init_vssram_gpa_range(void)
{
	uint64_t gpu_rsvmem_base_gpa = get_gpu_rsvmem_base_gpa();

	vssram_size = VSSRAM_MAX_SIZE;
	/* TODO: It is better to put one boundary between GPU region and SW SRAM
	 * for protection.
	 */
	vssram_gpa_base = ((gpu_rsvmem_base_gpa ? gpu_rsvmem_base_gpa : 0x80000000UL) -
			vssram_size) &  ~vssram_size;
}

/**
 * @brief Initialize guest CPU information, including pCPU
 *        bitmask, number of vCPU and local APIC IDs of vCPUs.
 *
 * @param ctx  Pointer to context of user VM.
 *
 * @return 0 on success and -1 on fail.
 */
static int init_guest_cpu_info(struct vmctx *ctx)
{
	uint64_t guest_pcpu_bitmask;

	guest_pcpu_bitmask = vm_get_cpu_affinity_dm();
	if (guest_pcpu_bitmask == 0) {
		pr_err("%s,Err: Invalid guest_pcpu_bitmask.\n", __func__);
		return -1;
	}
	pr_info("%s, guest_cpu_bitmask: 0x%x\n", __func__, guest_pcpu_bitmask);

	if (init_guest_lapicid_tbl(guest_pcpu_bitmask) < 0) {
		pr_err("%s,init guest lapicid table fail.\n", __func__);
		return -1;
	}

	guest_vcpu_num = bitmap_weight(guest_pcpu_bitmask);
	guest_pcpumask = guest_pcpu_bitmask;
	return 0;
}

/*
 * @pre init_vssram(ctx) == 0
 */
uint64_t get_vssram_gpa_base(void)
{
	return vssram_gpa_base;
}

/*
 * @pre init_vssram(ctx) == 0
 */
uint64_t get_vssram_size(void)
{
	return vssram_size;
}

/*
 * @pre init_vssram(ctx) == 0
 */
uint8_t *get_vssram_vrtct(void)
{
	return (uint8_t *)vrtct_table;
}

/**
 * @brief Initialize software SRAM device for user VM.
 *
 * @param ctx  Pointer to context of user VM.
 *
 * @return 0 on success and -1 on fail.
 */
int init_vssram(struct vmctx *ctx)
{
	int status = -1;
	struct tcc_memory_info mem_info;

	init_vssram_gpa_range();

	if (init_guest_cpu_info(ctx) < 0)
		return -1;

	if (tcc_driver_init_buffer_fd() < 0)
		return -1;

	/*
	 * By default, pCPU is allowed to request software SRAM buffer
	 * from given region in TCC buffer driver only if this pCPU is
	 * set in the target region's CPU affinity configuration.
	 *
	 * This check shall be disabled for software SRAM virtualization
	 * usage in ACRN service VM, because software SRAM buffers are
	 * requested by ACRN DM on behalf of user VM, but ACRN DM and
	 * user VM may run on different CPUs while the target software
	 * SRAM region may be configured only for pCPUs that user VM runs on.
	 */
	if (disable_tcc_strict_affinity_check() < 0)
		return -1;

	if (vssram_init_buffers() < 0) {
		pr_err("%s, initialize vssram buffer description failed.\n", __func__);
		return -1;
	}

	memset(&mem_info, 0, sizeof(struct tcc_memory_info));
	if (load_tcc_memory_info(&mem_info) < 0) {
		pr_err("%s, load TCC memory configurations failed.\n", __func__);
		goto exit;
	}

	if (vssram_prepare_buffers(&mem_info) < 0) {
		pr_err("%s, prepare vssram buffers failed.\n", __func__);
		goto exit;
	}

	if (vssram_ept_map_buffers(ctx) < 0) {
		pr_err("%s, setup EPT mapping for vssram buffers failed.\n", __func__);
		goto exit;
	}

	vrtct_table = create_vrtct();
	if (vrtct_table == NULL) {
		pr_err("%s, create vRTCT failed.", __func__);
		goto exit;
	}
	status = 0;

exit:
	if (mem_info.region_configs)
		free(mem_info.region_configs);

	if (mem_info.cpuset)
		free(mem_info.cpuset);

	if (status < 0) {
		vssram_close_buffers();
		if (vssram_buffers) {
			free(vssram_buffers);
			vssram_buffers = NULL;
		}
	}
	return status;
}

/**
 * @brief De-initialize software SRAM device
 *
 * @param ctx  Pointer to context of user VM.
 *
 * @return void
 */
void deinit_vssram(struct vmctx *ctx)
{
	vssram_close_buffers();
	if (vssram_buffers) {
		free(vssram_buffers);
		vssram_buffers = NULL;
	}

	if (vrtct_table != NULL) {
		free(vrtct_table);
		vrtct_table = NULL;
	}
}

/**
 * @brief Cleanup vSSRAM configurations resource.
 *
 * @param  void
 *
 * @return void
 */
void clean_vssram_configs(void)
{
	if (vssram_buf_params) {
		free(vssram_buf_params);
		vssram_buf_params = NULL;
	}
}

/**
 * @brief Parse input args for software SRAM configurations.
 *
 * @param opt  Pointer input args string.
 *
 * @return 0 on success and non-zero on fail.
 *
 * @note  Format of args shall be: "--ssram {Ln,vcpu=vcpu_set,size=nK|M;}"
 *        - "Ln,vcpu=vcpu_set,size=nK|M;" is used to configure one software SRAM region,
 *               multiple regions can be configured by separating them with semicolons(;).
 *        - Ln   Cache level of software region, "L2"/"l3" and "L3"/"l3" are valid values
 *               for L2 and L3 cache respectively.
 *        - vcpu vCPU set of software region, multiple vCPU IDs can be configured by
 *               separating them with comma(,), "all"/"ALL" can be used to set all vCPUs.
 *        - size Size of software SRAM region, suffix of "K"/"k" or "M"/"m" must be used
 *               to indicate unit of Kilo bytes and Mega bytes respectively, this value
 *               must be decimal and page size(4Kbytes) aligned.
 *        - example: --ssram L2,vcpu=0,1,size=4K;L2,vcpu=2,3,size=1M;L3,vcpu=all,size=2M
 */
int parse_vssram_buf_params(const char *opt)
{
	size_t size;
	uint32_t vcpu_id;
	uint64_t vcpumask;
	int level, shift, error = -1, index = 0;
	struct vssram_buf_param *params;
	char *cp_opt, *str, *elem, *s_elem, *level_str, *vcpu_str, *size_str, *endptr;

	cp_opt = str = strdup(opt);
	if (!str) {
		fprintf(stderr, "%s: strdup returns NULL.\n", __func__);
		return -1;
	}

	params = calloc(MAX_VSSRAM_BUFFER_NUM, sizeof(struct vssram_buf_param));
	if (params == NULL) {
		pr_err("%s malloc buffer.\n", __func__);
		goto exit;
	}

	/* param example: --ssram L2,vcpu=0,1,size=4K;L2,vcpu=2,3,size=1M;L3,vcpu=all,size=2M */
	for (elem = strsep(&str, ";"); elem != NULL; elem = strsep(&str, ";")) {
		if (strlen(elem) == 0)
			break;

		level_str = strsep(&elem, ",");
		if (!strcmp(level_str, "L2") || !strcmp(level_str, "l2"))
			level = L2_CACHE;
		else if (!strcmp(level_str, "L3") || !strcmp(level_str, "l3"))
			level = L3_CACHE;
		else {
			pr_err("invalid SSRAM buffer level:%s.\n", level_str);
			goto exit;
		}

		vcpu_str = strsep(&elem, "=");
		if (strcmp(vcpu_str, "vcpu")) {
			pr_err("%s is invalid, 'vcpu' must be specified.\n", vcpu_str);
			goto exit;
		}
		size_str = strstr(elem, "size=");
		if (size_str == NULL) {
			pr_err("invalid size parameter: %s\n", elem);
			goto exit;
		}

		/* split elem into vcpu ID list string and size string */
		*(size_str - 1) = '\0';
		vcpu_str = elem;
		vcpumask = 0;
		for (s_elem = strsep(&vcpu_str, ","); s_elem != NULL; s_elem = strsep(&vcpu_str, ",")) {
			if (strlen(s_elem) == 0)
				break;

			if (!strcmp(s_elem, "all") || !strcmp(s_elem, "ALL")) {
				vcpumask = VSSRAM_VCPUMASK_ALL;
				break;
			}

			if (dm_strtoui(s_elem, &endptr, 10, &vcpu_id)) {
				pr_err("invalid '%s' to specify vcpu ID.\n", s_elem);
				goto exit;
			}
			vcpumask |= (1 << vcpu_id);
		}
		if (bitmap_weight(vcpumask) == 0) {
			pr_err("vCPU bitmask of ssram region is not set.\n");
			goto exit;
		}

		strsep(&size_str, "=");
		if (strlen(size_str) == 0) {
			pr_err("invalid size configuration.\n");
			goto exit;
		}
		size = strtoul(size_str, &endptr, 0);
		switch (tolower((unsigned char)*endptr)) {
		case 'm':
			shift = 20;
			break;
		case 'k':
			shift = 10;
			break;
		default:
			pr_err("invalid size of '%s', only 'K','k'(KB) or 'M','m'(MB) can be suffixed!\n", size_str);
			goto exit;
		}

		size <<= shift;
		if ((size == 0) || ((size & ~PAGE_MASK) != 0) || (size > VSSRAM_MAX_SIZE)) {
			pr_err("size 0x%lx is invalid, 0 or not page-aligned, or too large.\n", size);
			goto exit;
		}

		pr_info("config index[%d]: cache level:%d, size:%lx, vcpumask:%lx\n", index, level, size, vcpumask);
		params[index].level = level;
		params[index].size = size;
		params[index].vcpumask = vcpumask;
		index++;
	}
	vssram_buf_params = params;
	error = 0;

exit:
	if (error) {
		free(params);
	}
	free(cp_opt);
	return error;
}
