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

static inline uint32_t lapicid_to_cpuid(uint32_t lapicid)
{
	/* hardcode for TGL & EHL platform */
	return (lapicid >> 1);
}

static inline uint32_t vcpuid_to_vlapicid(uint32_t vcpuid)
{
	/* keep vlapic same with vcpuid */
	return vcpuid;
}

/**
 * @brief Build a vLAPIC ID table base on the pCPU bitmask of guest
 *        and pCPU bitmask of Software SRAM.
 *
 * @param sw_sram_pcpu_bitmask    pCPU bitmask of Software SRAM region.
 * @param guest_pcpu_bitmask    pCUP bitmask of guest.
 * @param vlapicid_tbl   Pointer to buffer of vLAPIC ID table, caller shall
 *                       guarantee it will not overflow.
 *
 * @return number of vlapic IDs filled to buffer.
 */
static int map_software_sram_vlapic_ids(uint64_t sw_sram_pcpu_bitmask, uint64_t guest_pcpu_bitmask,
	uint32_t *vlapicid_tbl)
{
	uint32_t vcpuid = 0;
	int i, vlapicid_num = 0;
	int guest_pcpu_index_start = ffsl(guest_pcpu_bitmask) - 1;
	int guest_pcpu_index_end = flsl(guest_pcpu_bitmask) - 1;

	for (i = guest_pcpu_index_start; i <= guest_pcpu_index_end; i++) {
		if ((guest_pcpu_bitmask & BITMASK(i)) != 0) {
			if ((sw_sram_pcpu_bitmask & BITMASK(i)) != 0) {
				vlapicid_tbl[vlapicid_num++] = vcpuid_to_vlapicid(vcpuid);
			}
			vcpuid++;
		}
	}
	return vlapicid_num;
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
 * @param sw_sram_pcpu_bitmask    pCPU bitmask of Software SRAM region.
 * @param guest_pcpu_bitmask    pCUP bitmask of guest.
 *
 * @return 0 on success and non-zero on fail.
 */
static int vrtct_add_ssram_entry(struct acpi_table_hdr *vrtct, uint32_t cache_level, uint64_t base, uint32_t ways,
					uint32_t size, uint64_t sw_sram_pcpu_bitmask, uint64_t guest_pcpu_bitmask)
{
	int vlapicid_num;
	struct rtct_entry *rtct_entry;
	struct rtct_entry_data_ssram *sw_sram;

	rtct_entry = get_free_rtct_entry(vrtct);
	rtct_entry->format = 1;
	rtct_entry->type = RTCT_ENTRY_TYPE_SSRAM;

	sw_sram =  (struct rtct_entry_data_ssram *)rtct_entry->data;
	sw_sram->cache_level = cache_level;
	sw_sram->base = base;
	sw_sram->ways = ways;
	sw_sram->size = size;

	vlapicid_num = map_software_sram_vlapic_ids(sw_sram_pcpu_bitmask, guest_pcpu_bitmask, sw_sram->apic_id_tbl);
	if (vlapicid_num <= 0)
		return -1;

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
static int vrtct_add_mem_hierarchy_entry(struct acpi_table_hdr *vrtct, uint32_t hierarchy,
		uint32_t clock_cycles, uint64_t vcpu_num)
{
	int vcpuid;
	struct rtct_entry *rtct_entry;
	struct rtct_entry_data_mem_hi_latency *mem_hi;

	rtct_entry = get_free_rtct_entry(vrtct);
	rtct_entry->format = 1;
	rtct_entry->type = RTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY;

	mem_hi = (struct rtct_entry_data_mem_hi_latency *)rtct_entry->data;
	mem_hi->hierarchy = hierarchy;
	mem_hi->clock_cycles = clock_cycles;

	for (vcpuid = 0; vcpuid < vcpu_num; vcpuid++) {
		mem_hi->apic_id_tbl[vcpuid] = vcpuid_to_vlapicid(vcpuid);
	}
	rtct_entry->size = RTCT_MEM_HI_HEADER_SIZE +  (vcpu_num * sizeof(uint32_t));

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
 * @brief Initialize Software SRAM and memory hierarchy entries in virtual RTCT,
 *        configurations of these entries are from native RTCT.
 *
 * @param vrtct        Pointer to virtual RTCT.
 * @param native_rtct  Pointer to native RTCT.
 * @param guest_pcpu_bitmask    pCUP bitmask of guest.
 *
 * @return 0 on success and non-zero on fail.
 */
static int passthru_rtct_to_guest(struct acpi_table_hdr *vrtct, struct acpi_table_hdr *native_rtct,
				uint64_t guest_pcpu_bitmask)
{
	int i, cpu_num, rc = 0;
	uint64_t sw_sram_pcpu_bitmask;
	struct rtct_entry *entry;
	struct rtct_entry_data_ssram *sw_sram;
	struct rtct_entry_data_mem_hi_latency *mem_hi;

	foreach_rtct_entry(native_rtct, entry) {
		switch (entry->type) {
		case RTCT_ENTRY_TYPE_SSRAM:
			{
				/* Get native CPUs of Software SRAM region */
				cpu_num = (entry->size - RTCT_SSRAM_HEADER_SIZE) / sizeof(uint32_t);
				sw_sram =  (struct rtct_entry_data_ssram *)entry->data;
				sw_sram_pcpu_bitmask = 0;
				for (i = 0; i < cpu_num; i++) {
					sw_sram_pcpu_bitmask |= (1U << lapicid_to_cpuid(sw_sram->apic_id_tbl[i]));
				}

				if ((sw_sram_pcpu_bitmask & guest_pcpu_bitmask) != 0) {
					/*
					 * argument 'base' is set to HPA(sw_sram->base) in passthru RTCT
					 * soluation as it is required to calculate Software SRAM regions range
					 * in host physical address space, this 'base' will be updated to
					 * GPA when mapping all Software SRAM regions from HPA to GPA.
					 */
					rc = vrtct_add_ssram_entry(vrtct, sw_sram->cache_level, sw_sram->base,
						sw_sram->ways, sw_sram->size, sw_sram_pcpu_bitmask, guest_pcpu_bitmask);
				}
			}
			break;

		case RTCT_ENTRY_TYPE_MEM_HIERARCHY_LATENCY:
			{
				mem_hi = (struct rtct_entry_data_mem_hi_latency *)entry->data;
				rc = vrtct_add_mem_hierarchy_entry(vrtct, mem_hi->hierarchy, mem_hi->clock_cycles,
					bitmap_weight(guest_pcpu_bitmask));
			}
			break;

		default:
			break;
		}

		if (rc)
			break;
	}

	if (rc)
		return -1;

	remap_software_sram_regions(vrtct);
	vrtct->checksum = vrtct_checksum((uint8_t *)vrtct, vrtct->length);
	return  rc;
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

	if (vm_get_config(ctx, &vm_cfg)) {
		pr_err("%s, get VM configuration fail.\n", __func__);
		goto error;
	}

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

	gpu_rsvmem_base_gpa = get_gpu_rsvmem_base_gpa();
	software_sram_size = SOFTWARE_SRAM_MAX_SIZE;
	/* TODO: It is better to put one boundary between GPU region and SW SRAM
	 * for protection.
	 */
	software_sram_base_gpa = ((gpu_rsvmem_base_gpa ? gpu_rsvmem_base_gpa : 0x80000000UL) -
			software_sram_size) &  ~software_sram_size;

	if (passthru_rtct_to_guest(vrtct, rtct_cfg, guest_pcpu_bitmask)) {
		pr_err("%s, initialize vRTCT fail.", __func__);
		goto error;
	}

	return (uint8_t *)vrtct;
error:
	free(vrtct);
	return NULL;
}
