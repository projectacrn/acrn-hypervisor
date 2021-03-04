/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <x86/lib/bits.h>
#include <rtl.h>
#include <logmsg.h>
#include <misc_cfg.h>
#include <x86/mmu.h>
#include <x86/rtcm.h>


static uint64_t software_sram_bottom_hpa;
static uint64_t software_sram_top_hpa;

/* is_sw_sram_initialized is used to tell whether Software SRAM is successfully initialized for all cores */
volatile bool is_sw_sram_initialized = false;

#ifdef CONFIG_PSRAM_ENABLED

static struct rtct_entry_data_rtcm_binary *rtcm_binary = NULL;

static struct acpi_table_header *acpi_rtct_tbl = NULL;

static inline void rtcm_set_nx(bool add)
{
	ppt_set_nx_bit((uint64_t)hpa2hva(rtcm_binary->address), rtcm_binary->size, add);
}

static inline void rtcm_flush_binary_tlb(void)
{
	uint64_t linear_addr, start_addr = (uint64_t)hpa2hva(rtcm_binary->address);
	uint64_t end_addr = start_addr + rtcm_binary->size;

	for (linear_addr = start_addr; linear_addr < end_addr; linear_addr += PAGE_SIZE) {
		invlpg(linear_addr);
	}

}

static inline void *get_rtct_entry_base()
{
	return (void *)acpi_rtct_tbl + sizeof(*acpi_rtct_tbl);
}

void set_rtct_tbl(void *rtct_tbl_addr)
{
	acpi_rtct_tbl = rtct_tbl_addr;
}

/*
 *@pre the PSRAM region is separate and never mixed with normal DRAM
 *@pre acpi_rtct_tbl != NULL
 */
static void parse_rtct(void)
{
	uint64_t bottom_hpa = ULONG_MAX;
	struct rtct_entry *entry;
	struct rtct_entry_data_software_sram *sw_sram_entry;

	entry = get_rtct_entry_base();
	while (((uint64_t)entry - (uint64_t)acpi_rtct_tbl) < acpi_rtct_tbl->length) {
		switch (entry->type) {
		case RTCT_ENTRY_TYPE_RTCM_BINARY:
			rtcm_binary = (struct rtct_entry_data_rtcm_binary *)entry->data;
			ASSERT((rtcm_binary->address != 0UL && rtcm_binary->size != 0U), "Invalid PTCM binary.");
			pr_info("found RTCM bin, in HPA %llx, size %llx", rtcm_binary->address, rtcm_binary->size);
			break;

		case RTCT_ENTRY_TYPE_SOFTWARE_SRAM:
			sw_sram_entry = (struct rtct_entry_data_software_sram *)entry->data;
			if (software_sram_top_hpa < sw_sram_entry->base + sw_sram_entry->size) {
				software_sram_top_hpa = sw_sram_entry->base + sw_sram_entry->size;
			}
			if (bottom_hpa > sw_sram_entry->base) {
				bottom_hpa = sw_sram_entry->base;
			}
			pr_info("found L%d Software SRAM, at HPA %llx, size %x", sw_sram_entry->cache_level,
				sw_sram_entry->base, sw_sram_entry->size);
			break;
		/* In current phase, we ignore other entries like gt_clos and wrc_close */
		default:
			break;
		}
		/* point to next rtct entry */
		entry = (struct rtct_entry *)((uint64_t)entry + entry->size);
	}

	if (bottom_hpa != ULONG_MAX) {
		/* Software SRAM regions are detected. */
		software_sram_bottom_hpa = bottom_hpa;
		software_sram_top_hpa = round_page_up(software_sram_top_hpa);
	}
}

/*
 * Function to initialize Software SRAM. Both BSP and APs shall call this function to
 * make sure Software SRAM is initialized, which is required by RTCM.
 * BSP:
 *	To parse RTCT and find the entry of RTCM command function
 * AP:
 *	Wait until BSP has done the parsing work, then call the RTCM ABI.
 *
 * Synchronization of AP and BSP is ensured, both inside and outside RTCM.
 * BSP shall be the last to finish the call.
 */
void init_software_sram(bool is_bsp)
{
	int32_t rtcm_ret_code;
	struct rtcm_header *header;
	rtcm_abi_func rtcm_command_func = NULL;
	static uint64_t init_sw_sram_cpus_mask = (1UL << BSP_CPU_ID);

	/*
	 * When we shut down an RTVM, its pCPUs will be re-initialized
	 * we must ensure init_software_sram() will only be executed at the first time when a pcpu is booted
	 * That's why we add "!is_sw_sram_initialized" as an condition.
	 */
	if (!is_sw_sram_initialized && (acpi_rtct_tbl != NULL)) {
		/* TODO: We may use SMP call to flush TLB and do Software SRAM initialization on APs */
		if (is_bsp) {
			parse_rtct();
			if (rtcm_binary != NULL) {
				/* Clear the NX bit of PTCM area */
				rtcm_set_nx(false);
			}
			bitmap_clear_lock(get_pcpu_id(), &init_sw_sram_cpus_mask);
		}

		wait_sync_change(&init_sw_sram_cpus_mask, 0UL);
		if (rtcm_binary != NULL) {
			header = hpa2hva(rtcm_binary->address);
			pr_info("rtcm_bin_address:%llx, rtcm magic:%x, rtcm version:%x",
				rtcm_binary->address, header->magic, header->version);
			ASSERT(header->magic == RTCM_MAGIC, "Incorrect RTCM magic!");

			/* Flush the TLB, so that BSP/AP can execute the RTCM ABI */
			rtcm_flush_binary_tlb();
			rtcm_command_func = (rtcm_abi_func)(hpa2hva(rtcm_binary->address) + header->command_offset);
			pr_info("rtcm command function is found at %llx", rtcm_command_func);
			rtcm_ret_code = rtcm_command_func(RTCM_CMD_INIT_SOFTWARE_SRAM, get_rtct_entry_base());
			pr_info("rtcm initialization return %d", rtcm_ret_code);
			/* return 0 for success, -1 for failure */
			ASSERT(rtcm_ret_code == 0);

			if (is_bsp) {
				/* Restore the NX bit of RTCM area in page table */
				rtcm_set_nx(true);
			}

			bitmap_set_lock(get_pcpu_id(), &init_sw_sram_cpus_mask);
			wait_sync_change(&init_sw_sram_cpus_mask, ALL_CPUS_MASK);
			/* Flush the TLB on BSP and all APs to restore the NX for Software SRAM area */
			rtcm_flush_binary_tlb();

			if (is_bsp) {
				is_sw_sram_initialized = true;
				pr_info("BSP Software SRAM has been initialized, base_hpa:0x%lx, top_hpa:0x%lx.\n",
					software_sram_bottom_hpa, software_sram_top_hpa);
			}
		}
	}
}
#else
void set_rtct_tbl(__unused void *rtct_tbl_addr)
{
}

void init_software_sram(__unused bool is_bsp)
{
}
#endif

uint64_t get_software_sram_base(void)
{
	return software_sram_bottom_hpa;
}

uint64_t get_software_sram_size(void)
{
	return (software_sram_top_hpa - software_sram_bottom_hpa);
}
