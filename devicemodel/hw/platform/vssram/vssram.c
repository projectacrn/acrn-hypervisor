/*
 * Copyright (C) 2021 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <sys/ioctl.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/user.h>
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

uint8_t vrtct_checksum(uint8_t *vrtct, uint32_t length)
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
 * @brief Release all vSSRAM buffers and close file
 *        descriptors of TCC cache buffers.
 *
 * @param void
 *
 * @return void
 */
static void vssram_close_buffers(void)
{
	/* to be implemented */
}

static int init_guest_lapicid_tbl(struct acrn_platform_info *platform_info, uint64_t guest_pcpu_bitmask)
{
	int pcpu_id = 0, vcpu_id = 0;
	int vcpu_num = bitmap_weight(guest_pcpu_bitmask);

	for (vcpu_id = 0; vcpu_id < vcpu_num; vcpu_id++) {
		pcpu_id = pcpuid_from_vcpuid(guest_pcpu_bitmask, vcpu_id);
		if (pcpu_id < 0)
			return -1;

		guest_lapicid_tbl[vcpu_id] = lapicid_from_pcpuid(platform_info, pcpu_id);
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
	struct acrn_vm_config_header vm_cfg;
	uint64_t dm_cpu_bitmask, hv_cpu_bitmask, guest_pcpu_bitmask;
	struct acrn_platform_info platform_info;

	if (vm_get_config(ctx, &vm_cfg, &platform_info)) {
		pr_err("%s, get VM configuration fail.\n", __func__);
		return -1;
	}
	assert(platform_info.hw.cpu_num <= ACRN_PLATFORM_LAPIC_IDS_MAX);

	/*
	 * pCPU bitmask of VM is configured in hypervisor by default but can be
	 * overwritten by '--cpu_affinity' argument of DM if this bitmask is
	 * the subset of bitmask configured in hypervisor.
	 *
	 * FIXME: The cpu_affinity does not only mean the vcpu's pcpu affinity but
	 * also indicates the maximum vCPU number of guest. Its name should be renamed
	 * to pu_bitmask to avoid confusing.
	 */
	hv_cpu_bitmask = vm_cfg.cpu_affinity;
	dm_cpu_bitmask = vm_get_cpu_affinity_dm();
	if ((dm_cpu_bitmask != 0) && ((dm_cpu_bitmask & ~hv_cpu_bitmask) == 0)) {
		guest_pcpu_bitmask = dm_cpu_bitmask;
	} else {
		guest_pcpu_bitmask = hv_cpu_bitmask;
	}

	if (guest_pcpu_bitmask == 0) {
		pr_err("%s,Err: Invalid guest_pcpu_bitmask.\n", __func__);
		return -1;
	}

	pr_info("%s, dm_cpu_bitmask:0x%x, hv_cpu_bitmask:0x%x, guest_cpu_bitmask: 0x%x\n",
		__func__, dm_cpu_bitmask, hv_cpu_bitmask, guest_pcpu_bitmask);

	if (init_guest_lapicid_tbl(&platform_info, guest_pcpu_bitmask) < 0) {
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

	if (vssram_init_buffers() < 0) {
		pr_err("%s, initialize vssram buffer description failed.\n", __func__);
		return -1;
	}

	memset(&mem_info, 0, sizeof(struct tcc_memory_info));
	if (load_tcc_memory_info(&mem_info) < 0) {
		pr_err("%s, load TCC memory configurations failed.\n", __func__);
		goto exit;
	}

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
