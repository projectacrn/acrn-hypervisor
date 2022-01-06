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

#include "pci_core.h"
#include "vmmapi.h"
#include "acpi.h"
#include "log.h"
#include "vssram.h"

#define RTCT_V1 1
#define RTCT_V2 2

#define RTCT_ENTRY_HEADER_SIZE 8
#define foreach_rtct_entry(rtct, e) \
	for (e = (void *)rtct + sizeof(struct acpi_table_hdr); \
		((uint64_t)e - (uint64_t)rtct) < rtct->length; \
		e = (struct rtct_entry *)((uint64_t)e + e->size))

static uint16_t guest_vcpu_num;
static uint32_t guest_l2_cat_shift;
static uint32_t guest_l3_cat_shift;
static uint32_t guest_lapicid_tbl[ACRN_PLATFORM_LAPIC_IDS_MAX];

static uint64_t vssram_size;
static uint64_t vssram_gpa_base;
static struct acpi_table_hdr  *vrtct_table;

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

	for (vcpu_id = 0; vcpu_id < guest_vcpu_num; vcpu_id++) {
		pcpu_id = pcpuid_from_vcpuid(guest_pcpu_bitmask, vcpu_id);
		if (pcpu_id < 0)
			return -1;

		guest_lapicid_tbl[vcpu_id] = lapicid_from_pcpuid(platform_info, pcpu_id);
	}
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
#define PTCT_BUF_SIZE 4096
	struct acrn_vm_config_header vm_cfg;
	uint64_t dm_cpu_bitmask, hv_cpu_bitmask, guest_pcpu_bitmask;
	uint32_t gpu_rsvmem_base_gpa = 0;
	struct acrn_platform_info platform_info;

	if (vm_get_config(ctx, &vm_cfg, &platform_info)) {
		pr_err("%s, get VM configuration fail.\n", __func__);
		goto error;
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
		goto error;
	}

	pr_info("%s, dm_cpu_bitmask:0x%x, hv_cpu_bitmask:0x%x, guest_cpu_bitmask: 0x%x\n",
		__func__, dm_cpu_bitmask, hv_cpu_bitmask, guest_pcpu_bitmask);

	guest_vcpu_num = bitmap_weight(guest_pcpu_bitmask);
	if (init_guest_lapicid_tbl(&platform_info, guest_pcpu_bitmask) < 0) {
		pr_err("%s,init guest lapicid table fail.\n", __func__);
		goto error;
	}

	printf("%s, vcpu_num:%d, l2_shift:%d, l3_shift:%d.\n", __func__,
		guest_vcpu_num, guest_l2_cat_shift, guest_l3_cat_shift);

	gpu_rsvmem_base_gpa = get_gpu_rsvmem_base_gpa();
	vssram_size = VSSRAM_MAX_SIZE;
	/* TODO: It is better to put one boundary between GPU region and SW SRAM
	 * for protection.
	 */
	vssram_gpa_base = ((gpu_rsvmem_base_gpa ? gpu_rsvmem_base_gpa : 0x80000000UL) -
			vssram_size) &  ~vssram_size;

	return 0;
error:
	return -1;
}
