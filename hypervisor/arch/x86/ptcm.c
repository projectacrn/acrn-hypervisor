/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <logmsg.h>
#include <misc_cfg.h>
#include <mmu.h>
#include <ptcm.h>

uint64_t psram_area_bottom;
uint64_t psram_area_top;

#ifdef CONFIG_PTCM_ENABLED
struct ptcm_mem_region ptcm_mem_regions[MAX_PCPU_NUM];
struct ptct_entry_data_ptcm_binary ptcm_binary;
uint32_t l2_psram_regions_count = 0U;
uint32_t l3_psram_regions_count = 0U;
struct ptct_entry_data_psram l2_psram_regions[MAX_L2_PSRAM_REGIONS];
struct ptct_entry_data_psram l3_psram_regions[MAX_L3_PSRAM_REGIONS];

/*
	Function to parse PTCT, to find:
		1. The location of PTCM binary, and verify.
		2. Information about pSRAM, including address, size, and cache level.
*/
static void parse_ptct(void)
{
	struct ptct_entry *entry;
	struct ptcm_mem_region *p_mem_region;
	struct ptct_entry_data_psram *psram_entry;
	struct ptct_entry_data_ptcm_binary* ptcm_binary_entry;
	struct acpi_table_ptct* acpi_ptct = (struct acpi_table_ptct *)get_acpi_tbl(ACPI_SIG_PTCT);

	if (acpi_ptct == NULL) {
		pr_fatal("Cannot find PTCT pointer!!!!");
	} else {
		pr_info("found PTCT subtable in HPA %llx, length: %d", acpi_ptct, acpi_ptct->header.length);

		entry = &acpi_ptct->ptct_first_entry;
		psram_area_bottom = PSRAM_BASE_HPA;

		while (((uint64_t)entry - (uint64_t)acpi_ptct) < acpi_ptct->header.length) {
			switch (entry->type) {
			case PTCT_ENTRY_TYPE_PTCM_BINARY:
				ptcm_binary_entry = (struct ptct_entry_data_ptcm_binary *)entry->data;
				ptcm_binary.address = ptcm_binary_entry->address;
				ptcm_binary.size = ptcm_binary_entry->size;
				if (psram_area_top < ptcm_binary.address + ptcm_binary.size) {
					psram_area_top = ptcm_binary.address + ptcm_binary.size;
				}
				pr_info("found PTCM bin, in HPA %llx, size %llx", ptcm_binary.address, ptcm_binary.size);
				break;

			case PTCT_ENTRY_TYPE_PSRAM:
				psram_entry = (struct ptct_entry_data_psram *)entry->data;
				if (psram_entry->apic_id_0 >= MAX_PCPU_NUM) {
					break;
				}

				p_mem_region = &ptcm_mem_regions[psram_entry->apic_id_0];
				if (psram_entry->cache_level == 3) {
					if (l3_psram_regions_count >= MAX_L3_PSRAM_REGIONS) {
						pr_err("Too many L3 regions!");
						break;
					}

					p_mem_region->l3_valid = true;
					p_mem_region->l3_base = psram_entry->base;
					p_mem_region->l3_size = psram_entry->size;
					p_mem_region->l3_ways = psram_entry->ways;
					p_mem_region->l3_clos[PTCM_CLOS_INDEX_SETUP] = p_mem_region->l3_ways;
					p_mem_region->l3_clos[PTCM_CLOS_INDEX_LOCK] ^= p_mem_region->l3_ways;
					if (psram_area_top < p_mem_region->l3_base + p_mem_region->l3_size) {
						psram_area_top = p_mem_region->l3_base + p_mem_region->l3_size;
					}
					l3_psram_regions_count++;
					pr_info("found L3 psram, at HPA %llx, size %x", p_mem_region->l3_base, p_mem_region->l3_size);
				} else if (psram_entry->cache_level == 2) {
					if (l2_psram_regions_count >= MAX_L2_PSRAM_REGIONS) {
						pr_err("Too many L2 regions!");
						break;
					}
					p_mem_region->l2_valid = true;
					p_mem_region->l2_base = psram_entry->base;
					p_mem_region->l2_size = psram_entry->size;
					p_mem_region->l2_ways = psram_entry->ways;
					p_mem_region->l2_clos[PTCM_CLOS_INDEX_SETUP] = p_mem_region->l2_ways;
					p_mem_region->l2_clos[PTCM_CLOS_INDEX_LOCK] ^= p_mem_region->l2_ways;
					if (psram_area_top < p_mem_region->l2_base + p_mem_region->l2_size) {
						psram_area_top = p_mem_region->l2_base + p_mem_region->l2_size;
					}
					l2_psram_regions_count++;
					pr_info("found L2 psram, in HPA %llx, size %llx", psram_entry->base, psram_entry->size);
				}
				break;
			/* In current phase, we ignore other entries like gt_clos and wrc_close */
			default:
				break;
			}
			/* point to next ptct entry */
			entry = (struct ptct_entry *)((uint64_t)entry + entry->size);
		}
		psram_area_top = round_page_up(psram_area_top);
		psram_area_bottom = round_page_down(psram_area_bottom);
	}
}
#endif
