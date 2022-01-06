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
static uint16_t guest_vcpu_num;
static uint64_t guest_pcpumask;
uint32_t guest_l2_cat_shift;
uint32_t guest_l3_cat_shift;
static uint32_t guest_lapicid_tbl[ACRN_PLATFORM_LAPIC_IDS_MAX];

static uint64_t vssram_size;
static uint64_t vssram_gpa_base;
static struct acpi_table_hdr  *vrtct_table;

static struct   vssram_buf_param *vssram_buf_params;

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
 * @brief  Pass-through a native entry to virtual RTCT.
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
	init_vssram_gpa_range();

	if (init_guest_cpu_info(ctx) < 0)
		return -1;

	return 0;
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
