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
#include "rtct.h"

#define RTCT_ENTRY_HEADER_SIZE 8
#define RTCT_SSRAM_HEADER_SIZE  (RTCT_ENTRY_HEADER_SIZE + 20)
#define RTCT_MEM_HI_HEADER_SIZE  (RTCT_ENTRY_HEADER_SIZE + 8)

#define BITMASK(nr) (1U << nr)

#define foreach_rtct_entry(rtct, e) \
	for (e = (void *)rtct + sizeof(struct acpi_table_hdr); \
		((uint64_t)e - (uint64_t)rtct) < rtct->length; \
		e = (struct rtct_entry *)((uint64_t)e + e->size))

static uint16_t guest_vcpu_num;
static uint32_t guest_lapicid_tbl[MAX_PLATFORM_LAPIC_IDS];

static uint64_t software_sram_base_hpa;
static uint64_t software_sram_size;
static uint64_t software_sram_base_gpa;

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
 * @brief Add a new Software SRAM region entry to virtual RTCT.
 *
 * @param vrtct  Pointer to virtual RTCT.
 * @param cache_level    Cache level of Software SRAM region.
 * @param base    Base address of Software SRAM region.
 * @param ways    Cache ways of Software SRAM region.
 * @param size    Size of Software SRAM region.
 * @param vlapic_ids    vLAIC ID table base address.
 * @param vlapicid_num   Entry number of vLAPIC ID table.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vrtct_add_ssram_entry(struct acpi_table_hdr *vrtct, uint32_t cache_level, uint64_t base, uint32_t ways,
					uint32_t size, uint32_t *vlapic_ids, uint32_t vlapicid_num)
{
	struct rtct_entry *rtct_entry;
	struct rtct_entry_data_ssram *sw_sram;

	rtct_entry = get_free_rtct_entry(vrtct);
	rtct_entry->format_version = 1;
	rtct_entry->type = RTCT_ENTRY_TYPE_SSRAM;

	sw_sram =  (struct rtct_entry_data_ssram *)rtct_entry->data;
	sw_sram->cache_level = cache_level;
	sw_sram->base = base;
	sw_sram->ways = ways;
	sw_sram->size = size;

	memcpy(sw_sram->apic_id_tbl, vlapic_ids, vlapicid_num * sizeof(uint32_t));

	rtct_entry->size = RTCT_SSRAM_HEADER_SIZE + (vlapicid_num * sizeof(uint32_t));
	add_rtct_entry(vrtct, rtct_entry);
	return 0;
}

/**
 * @brief Add a memory hierarchy entry to virtual RTCT.
 *
 * @param vrtct  Pointer to virtual RTCT.
 * @param hierarchy     Memory hierarchy(2: cache level-2, 3:cache level-3, 256: the last level)
 * @param clock_cycles  Latency value of memory 'hierarchy'.
 * @param vcpu_num      Number of guest vCPU.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vrtct_add_mem_hierarchy_entry(struct acpi_table_hdr *vrtct, uint32_t hierarchy, uint32_t clock_cycles)
{
	uint32_t lapicid_tbl_sz;
	struct rtct_entry *rtct_entry;
	struct rtct_entry_data_mem_hi_latency *mem_hi;

	rtct_entry = get_free_rtct_entry(vrtct);
	rtct_entry->format_version = 1;
	rtct_entry->type = RTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY;

	mem_hi = (struct rtct_entry_data_mem_hi_latency *)rtct_entry->data;
	mem_hi->hierarchy = hierarchy;
	mem_hi->clock_cycles = clock_cycles;

	lapicid_tbl_sz = guest_vcpu_num * sizeof(uint32_t);
	memcpy(mem_hi->apic_id_tbl, guest_lapicid_tbl, lapicid_tbl_sz);

	rtct_entry->size = RTCT_MEM_HI_HEADER_SIZE + lapicid_tbl_sz;

	add_rtct_entry(vrtct, rtct_entry);
	return 0;
}

/**
 * @brief Update the base address of Software SRAM regions in vRTCT from
 *		  host physical address(HPA) to guest physical address(GPA).
 *
 * @param vrtct  Pointer to virtual RTCT.
 *
 * @return void
 */
static void remap_software_sram_regions(struct acpi_table_hdr *vrtct)
{
	struct rtct_entry *entry;
	struct rtct_entry_data_ssram *sw_sram_region;
	uint64_t hpa_bottom, hpa_top;

	hpa_bottom = (uint64_t)-1;
	hpa_top = 0;

	foreach_rtct_entry(vrtct, entry) {
		if (entry->type == RTCT_ENTRY_TYPE_SSRAM) {
			sw_sram_region = (struct rtct_entry_data_ssram *)entry->data;
			if (hpa_bottom > sw_sram_region->base) {
				hpa_bottom = sw_sram_region->base;
			}

			if (hpa_top < sw_sram_region->base + sw_sram_region->size) {
				hpa_top = sw_sram_region->base + sw_sram_region->size;
			}
		}
	}
	pr_info("%s, hpa_bottom:%lx, hpa_top:%lx.\n", __func__, hpa_bottom, hpa_top);

	software_sram_base_hpa = hpa_bottom;
	software_sram_size = hpa_top - hpa_bottom;

	foreach_rtct_entry(vrtct, entry) {
		if (entry->type == RTCT_ENTRY_TYPE_SSRAM) {
			sw_sram_region = (struct rtct_entry_data_ssram *)entry->data;
			sw_sram_region->base = software_sram_base_gpa + (sw_sram_region->base - hpa_bottom);
		}
	}
}

/**
 * @brief  Check if a given pCPU is assigned to current guest.
 *
 * @param lapicid  Physical LAPIC ID of pCPU.
 *
 * @return true if given pCPU is assigned to current guest, else false.
 */
static bool is_pcpu_assigned_to_guest(uint32_t lapicid)
{
	int i;

	for (i = 0; i < guest_vcpu_num; i++) {
		if (lapicid == guest_lapicid_tbl[i])
			return true;
	}
	return false;
}

/**
 * @brief Initialize Software SRAM and memory hierarchy entries in virtual RTCT,
 *        configurations of these entries are from native RTCT.
 *
 * @param vrtct        Pointer to virtual RTCT.
 * @param native_rtct  Pointer to native RTCT.
 *
 * @return 0 on success and non-zero on fail.
 */
static int passthru_rtct_to_guest(struct acpi_table_hdr *vrtct, struct acpi_table_hdr *native_rtct)
{
	int i, plapic_num, vlapic_num, rc = 0;
	struct rtct_entry *entry;
	struct rtct_entry_data_ssram *ssram;
	struct rtct_entry_data_mem_hi_latency *mem_hi;
	uint32_t lapicids[MAX_PLATFORM_LAPIC_IDS];

	foreach_rtct_entry(native_rtct, entry) {
		if (entry->type == RTCT_ENTRY_TYPE_SSRAM) {
			/* Get native CPUs of Software SRAM region */
			plapic_num = (entry->size - RTCT_SSRAM_HEADER_SIZE) / sizeof(uint32_t);
			ssram =  (struct rtct_entry_data_ssram *)entry->data;

			memset(lapicids, 0, sizeof(lapicids[MAX_PLATFORM_LAPIC_IDS]));
			vlapic_num = 0;
			for (i = 0; i < plapic_num; i++) {
				if (is_pcpu_assigned_to_guest(ssram->apic_id_tbl[i])) {
					lapicids[vlapic_num++] = ssram->apic_id_tbl[i];
				}
			}

			if (vlapic_num > 0) {
				/*
				 * argument 'base' is set to HPA(ssram->base) in passthru RTCT
				 * soluation as it is required to calculate Software SRAM regions range
				 * in host physical address space, this 'base' will be updated to
				 * GPA when mapping all Software SRAM regions from HPA to GPA.
				 */
				rc = vrtct_add_ssram_entry(vrtct, ssram->cache_level, ssram->base,
					ssram->ways, ssram->size, lapicids, vlapic_num);
			}
		} else if (entry->type == RTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY) {
			mem_hi = (struct rtct_entry_data_mem_hi_latency *)entry->data;
			rc = vrtct_add_mem_hierarchy_entry(vrtct, mem_hi->hierarchy, mem_hi->clock_cycles);
		}

		if (rc)
			return -1;
	}

	remap_software_sram_regions(vrtct);
	vrtct->checksum = vrtct_checksum((uint8_t *)vrtct, vrtct->length);
	return  0;
}

static int init_guest_lapicid_tbl(struct platform_info *platform_info, uint64_t guest_pcpu_bitmask)
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
 * @pre buid_vrtct(ctx, cfg) != NULL
 */
uint64_t get_software_sram_base_hpa(void)
{
	return software_sram_base_hpa;
}

/*
 * @pre buid_vrtct(ctx, cfg) != NULL
 */
uint64_t get_software_sram_base_gpa(void)
{
	return software_sram_base_gpa;
}

/*
 * @pre buid_vrtct(ctx, cfg) != NULL
 */
uint64_t get_software_sram_size(void)
{
	return software_sram_size;
}

/**
 * @brief Initialize virtual RTCT based on configurations from native RTCT in Service VM.
 *
 * @param ctx  Pointer to context of guest.
 * @param cfg  Pointer to configuration data, it pointers to native RTCT in passthru RTCT solution.
 *
 * @return Pointer to virtual RTCT data on success and NULL on fail.
 */
uint8_t *build_vrtct(struct vmctx *ctx, void *cfg)
{
#define PTCT_BUF_SIZE 4096
	struct acrn_vm_config vm_cfg;
	struct acpi_table_hdr *rtct_cfg, *vrtct = NULL;
	uint64_t dm_cpu_bitmask, hv_cpu_bitmask, guest_pcpu_bitmask;
	uint32_t gpu_rsvmem_base_gpa = 0;
	struct platform_info platform_info;

	if ((cfg == NULL) || (ctx == NULL))
		return NULL;

	rtct_cfg = (struct acpi_table_hdr *)cfg;
	vrtct = malloc(PTCT_BUF_SIZE);
	if (vrtct == NULL) {
		pr_err("%s, Failed to allocate vRTCT buffer.\n", __func__);
		return NULL;
	}

	memcpy((void *)vrtct, (void *)rtct_cfg, sizeof(struct acpi_table_hdr));
	vrtct->length = sizeof(struct acpi_table_hdr);
	vrtct->checksum = 0;

	if (vm_get_config(ctx, &vm_cfg, &platform_info)) {
		pr_err("%s, get VM configuration fail.\n", __func__);
		goto error;
	}
	assert(platform_info.cpu_num <= MAX_PLATFORM_LAPIC_IDS);

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

	gpu_rsvmem_base_gpa = get_gpu_rsvmem_base_gpa();
	software_sram_size = SOFTWARE_SRAM_MAX_SIZE;
	/* TODO: It is better to put one boundary between GPU region and SW SRAM
	 * for protection.
	 */
	software_sram_base_gpa = ((gpu_rsvmem_base_gpa ? gpu_rsvmem_base_gpa : 0x80000000UL) -
			software_sram_size) &  ~software_sram_size;

	if (passthru_rtct_to_guest(vrtct, rtct_cfg)) {
		pr_err("%s, initialize vRTCT fail.", __func__);
		goto error;
	}

	return (uint8_t *)vrtct;
error:
	free(vrtct);
	return NULL;
}
