/*
 * Copyright (C) 2020-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <bits.h>
#include <rtl.h>
#include <util.h>
#include <logmsg.h>
#include <misc_cfg.h>
#include <asm/mmu.h>
#include <asm/cpu_caps.h>
#include <asm/rtcm.h>
#include <cpu.h>


static uint64_t ssram_bottom_hpa;
static uint64_t ssram_top_hpa;

/* is_sw_sram_initialized is used to tell whether Software SRAM is successfully initialized for all cores */
static volatile bool is_sw_sram_initialized = false;

#ifdef CONFIG_SSRAM_ENABLED

#define foreach_rtct_entry(rtct, e) \
	for (e = (void *)rtct + sizeof(struct acpi_table_header); \
		((uint64_t)e - (uint64_t)rtct) < rtct->length; \
		e = (struct rtct_entry *)((uint64_t)e + e->size))

#define RTCT_V1 1U
#define RTCT_V2 2U
static uint32_t rtct_version = RTCT_V1;

static struct rtct_entry_data_rtcm_binary *rtcm_binary = NULL;

static struct acpi_table_header *acpi_rtct_tbl = NULL;

static inline void *get_rtct_entry_base()
{
	return (void *)acpi_rtct_tbl + sizeof(*acpi_rtct_tbl);
}

void set_rtct_tbl(void *rtct_tbl_addr)
{
	acpi_rtct_tbl = rtct_tbl_addr;
}

/*
 *@desc This function parses native RTCT ACPI talbe to figure out the entry to CRL binary
 *	and SSRAM range. All SSRAM regions shall be continuous and L3 cache be inclusive.
 *
 *@pre the SSRAM region is separate and never mixed with normal DRAM
 *@pre acpi_rtct_tbl != NULL
 */
static void parse_rtct(void)
{
	uint64_t bottom_hpa = ULONG_MAX;
	struct rtct_entry *entry;
	struct rtct_entry_data_ssram *ssram;
	struct rtct_entry_data_ssram_v2 *ssram_v2;
	struct rtct_entry_data_compatibility *compat;

	/* Check RTCT format */
	foreach_rtct_entry(acpi_rtct_tbl, entry) {
		if (entry->type == RTCT_V2_COMPATIBILITY) {
			compat = (struct rtct_entry_data_compatibility *)entry->data;
			rtct_version = compat->rtct_ver_major;
			break;
		}
	}
	pr_info("RTCT Version: V%d.\n", rtct_version);

	if (rtct_version == RTCT_V1) {
		foreach_rtct_entry(acpi_rtct_tbl, entry) {
			if (entry->type == RTCT_ENTRY_TYPE_SOFTWARE_SRAM) {
				ssram = (struct rtct_entry_data_ssram *)entry->data;

				ssram_top_hpa = max(ssram_top_hpa, ssram->base + ssram->size);
				bottom_hpa = min(bottom_hpa, ssram->base);
				pr_info("found L%d Software SRAM, at HPA %llx, size %x",
					ssram->cache_level, ssram->base, ssram->size);
			} else if (entry->type == RTCT_ENTRY_TYPE_RTCM_BINARY) {
				rtcm_binary = (struct rtct_entry_data_rtcm_binary *)entry->data;
				ASSERT((rtcm_binary->address != 0UL && rtcm_binary->size != 0U),
					"Invalid PTCM binary.");
			}
		}
	} else if (rtct_version == RTCT_V2) {
		foreach_rtct_entry(acpi_rtct_tbl, entry) {
			if (entry->type == RTCT_V2_SSRAM) {
				ssram_v2 = (struct rtct_entry_data_ssram_v2 *)entry->data;

				ssram_top_hpa = max(ssram_top_hpa, ssram_v2->base + ssram_v2->size);
				bottom_hpa = min(bottom_hpa, ssram_v2->base);
				pr_info("found L%d Software SRAM, at HPA %llx, size %x",
					ssram_v2->cache_level, ssram_v2->base, ssram_v2->size);
			} else if (entry->type == RTCT_V2_CRL_BINARY) {
				rtcm_binary = (struct rtct_entry_data_rtcm_binary *)entry->data;
				ASSERT((rtcm_binary->address != 0UL && rtcm_binary->size != 0U),
					"Invalid PTCM binary.");
			}
		}
	}

	if (bottom_hpa != ULONG_MAX) {
		/* Software SRAM regions are detected. */
		ssram_bottom_hpa = bottom_hpa;
		ssram_top_hpa = round_page_up(ssram_top_hpa);
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
bool init_software_sram(bool is_bsp)
{
	bool ret = true;
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
				set_paging_x((uint64_t)hpa2hva(rtcm_binary->address), rtcm_binary->size);
			}
			bitmap_clear(get_pcpu_id(), &init_sw_sram_cpus_mask);
		}

		wait_sync_change(&init_sw_sram_cpus_mask, 0UL);
		if (rtcm_binary != NULL) {
			header = hpa2hva(rtcm_binary->address);
			pr_info("rtcm_bin_address:%llx, rtcm magic:%x, rtcm version:%x",
				rtcm_binary->address, header->magic, header->version);
			ASSERT(((header->magic == RTCM_MAGIC_PTCM) || (header->magic == RTCM_MAGIC_RTCM)),
				"Wrong RTCM magic value!");

			/* Flush the TLB, so that BSP/AP can execute the RTCM ABI */
			flush_tlb_range((uint64_t)hpa2hva(rtcm_binary->address), rtcm_binary->size);
			rtcm_command_func = (rtcm_abi_func)(hpa2hva(rtcm_binary->address) + header->command_offset);
			pr_info("rtcm command function is found at %llx", rtcm_command_func);
			rtcm_ret_code = rtcm_command_func(RTCM_CMD_INIT_SOFTWARE_SRAM, get_rtct_entry_base());
			pr_info("rtcm initialization return %d", rtcm_ret_code);
			/* return 0 for success, -1 for failure */
			ASSERT(rtcm_ret_code == 0);

			if (is_bsp) {
				/* Restore the NX bit of RTCM area in page table */
				set_paging_nx((uint64_t)hpa2hva(rtcm_binary->address), rtcm_binary->size);
			}

			bitmap_set(get_pcpu_id(), &init_sw_sram_cpus_mask);
			wait_sync_change(&init_sw_sram_cpus_mask, ALL_CPUS_MASK);
			/* Flush the TLB on BSP and all APs to restore the NX for Software SRAM area */
			flush_tlb_range((uint64_t)hpa2hva(rtcm_binary->address), rtcm_binary->size);

			if (is_bsp) {
				is_sw_sram_initialized = true;
				pr_info("BSP Software SRAM has been initialized, base_hpa:0x%lx, top_hpa:0x%lx.\n",
					ssram_bottom_hpa, ssram_top_hpa);
			}
			ret = disable_host_monitor_wait();
		}
	}
	return ret;
}
#else
void set_rtct_tbl(__unused void *rtct_tbl_addr)
{
}

bool init_software_sram(__unused bool is_bsp)
{
	return true;
}
#endif

/* @pre called after 'init_software_sram()' done. */
bool is_software_sram_enabled(void)
{
	return is_sw_sram_initialized;
}

uint64_t get_software_sram_base(void)
{
	return ssram_bottom_hpa;
}

uint64_t get_software_sram_size(void)
{
	return (ssram_top_hpa - ssram_bottom_hpa);
}
