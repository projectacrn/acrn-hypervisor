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
volatile bool psram_is_initialized = false;

#ifdef CONFIG_PTCM_ENABLED

struct ptcm_mem_region ptcm_mem_regions[MAX_PCPU_NUM];
struct ptct_entry_data_ptcm_binary ptcm_binary;
uint32_t l2_psram_regions_count = 0U;
uint32_t l3_psram_regions_count = 0U;
struct ptct_entry_data_psram l2_psram_regions[MAX_L2_PSRAM_REGIONS];
struct ptct_entry_data_psram l3_psram_regions[MAX_L3_PSRAM_REGIONS];

static void parse_ptct(void)
{
	struct ptct_entry *entry;
	struct ptcm_mem_region *p_mem_region;
	struct ptct_entry_data_psram *psram_entry;
	struct ptct_entry_data_ptcm_binary* ptcm_binary_entry;
	struct acpi_table_ptct* acpi_ptct = (struct acpi_table_ptct *)get_acpi_tbl(ACPI_SIG_PTCT);

	if (acpi_ptct == NULL){
		pr_fatal("Cannot find PTCT pointer!!!!");
	}
	else {
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
					if (l3_psram_regions_count >= MAX_L3_PSRAM_REGIONS){
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
					pr_info("found L3 psram, in HPA %llx, size %llx", p_mem_region->l3_base, p_mem_region->l3_size);
				} else if (psram_entry->cache_level == 2) {
					if (l2_psram_regions_count >= MAX_L2_PSRAM_REGIONS){
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

static volatile uint64_t ptcm_command_interface_offset;
static volatile struct ptct_entry* ptct_base_entry;
static volatile ptcm_command_abi ptcm_command_interface = NULL;
/* psram_is_initialized is used to tell whether psram is successfully initialized for all cores */

int32_t init_psram(bool is_bsp)
{
	uint32_t magic,version;
	int32_t ptcm_ret_code;
	int ret = 0;

	/* When we shut down an RTVM, its pCPUs will be re-initialized
	we must ensure init_psram() will only be executed at the first time when a pcpu is booted */
	if (get_acpi_tbl(ACPI_SIG_PTCT) != NULL && psram_is_initialized == false) {
		if (is_bsp) {
			parse_ptct();
			pr_info("PTCT is parsed by BSP");
			ptct_base_entry = (struct ptct_entry*)((uint64_t)get_acpi_tbl(ACPI_SIG_PTCT) + 0x24);
			pr_info("ptct_base_entry is found by BSP at %llx", ptct_base_entry);
			magic = ((uint32_t *)ptcm_binary.address)[0];
			version = ((uint32_t *)ptcm_binary.address)[1];
			ptcm_command_interface_offset = *(uint64_t *)(ptcm_binary.address + 0x8);
			pr_info("ptcm_bin_address:%llx", ptcm_binary.address);
			pr_info("ptcm magic:%x", magic);
			pr_info("ptcm version:%x", version);

			if (magic != PTCM_MAGIC ){
				pr_err("Incorrect PTCM magic! Please turn off 'Above 4G MMIO Assignment' option in BIOS");
				psram_area_bottom = psram_area_top = 0;
				ptcm_command_interface = (ptcm_command_abi)(-1);
			}
			else
			{
				ptcm_command_interface = (ptcm_command_abi)(ptcm_binary.address + ptcm_command_interface_offset);
				pr_info("ptcm command interface is found at %llx",ptcm_command_interface);
			}
		} else {
			//all AP should wait until BSP finishes parsing PTCT and finding the command interface.
			while (!ptcm_command_interface) {
				continue;
			}
		}

		if ((int64_t)ptcm_command_interface != -1){ /* ptcm_command_interface is found */
			ptcm_ret_code = ptcm_command_interface(PTCM_CMD_INIT_PSRAM, (void *)ptct_base_entry);
			pr_info("ptcm initialization for core %d with return code %d", get_pcpu_id(), ptcm_ret_code);
			/* TODO: to handle the return errno gracefully */
			ASSERT(ptcm_ret_code == PTCM_STATUS_SUCCESS);
			/* wait until all cores finishes pSRAM initialization*/
			if (is_bsp){
				psram_is_initialized = true;
				pr_info("BSP pSRAM has been initialized\n");
			}
			else{
				while (!psram_is_initialized) {
					continue;
				}
			}
		}
		else {
			ret = -1;
		}
	}
	return ret;
}
#else
int32_t init_psram(__unused bool is_bsp)
{
	return -1;
}
#endif
