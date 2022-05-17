/*
 * Copyright (C) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <bits.h>
#include <logmsg.h>
#include <misc_cfg.h>
#include <mmu.h>
#include <ptcm.h>


uint64_t psram_area_bottom;
uint64_t psram_area_top;

/* is_psram_initialized is used to tell whether psram is successfully initialized for all cores */
volatile bool is_psram_initialized = false;

#ifdef CONFIG_PSRAM_ENABLED

static struct ptct_entry_data_ptcm_binary *ptcm_binary = NULL;

static struct acpi_table_header *acpi_ptct_tbl = NULL;

static inline void ptcm_set_nx(bool add)
{
	ppt_set_nx_bit((uint64_t)hpa2hva(ptcm_binary->address), ptcm_binary->size, add);
}

static inline void ptcm_flush_binary_tlb(void)
{
	uint64_t linear_addr, start_addr = (uint64_t)hpa2hva(ptcm_binary->address);
	uint64_t end_addr = start_addr + ptcm_binary->size;

	for (linear_addr = start_addr; linear_addr < end_addr; linear_addr += PAGE_SIZE) {
		invlpg(linear_addr);
	}

}


static inline void *get_ptct_address()
{
	return (void *)acpi_ptct_tbl + sizeof(*acpi_ptct_tbl);
}

void set_ptct_tbl(void *ptct_tbl_addr)
{
	acpi_ptct_tbl = ptct_tbl_addr;
}

static void parse_ptct(void)
{
	struct ptct_entry *entry;
	struct ptct_entry_data_psram *psram_entry;

	if (acpi_ptct_tbl != NULL) {
		pr_info("found PTCT subtable in HPA %llx, length: %d", acpi_ptct_tbl, acpi_ptct_tbl->length);

		entry = get_ptct_address();
		psram_area_bottom = PSRAM_BASE_HPA;

		while (((uint64_t)entry - (uint64_t)acpi_ptct_tbl) < acpi_ptct_tbl->length) {
			switch (entry->type) {
			case PTCT_ENTRY_TYPE_PTCM_BINARY:
				ptcm_binary = (struct ptct_entry_data_ptcm_binary *)entry->data;
				if (psram_area_top < ptcm_binary->address + ptcm_binary->size) {
					psram_area_top = ptcm_binary->address + ptcm_binary->size;
				}
				pr_info("found PTCM bin, in HPA %llx, size %llx", ptcm_binary->address, ptcm_binary->size);
				break;

			case PTCT_ENTRY_TYPE_PSRAM:
				psram_entry = (struct ptct_entry_data_psram *)entry->data;
				if (psram_area_top < psram_entry->base + psram_entry->size) {
					psram_area_top = psram_entry->base + psram_entry->size;
				}
				pr_info("found L%d psram, at HPA %llx, size %x", psram_entry->cache_level,
					psram_entry->base, psram_entry->size);
				break;
			/* In current phase, we ignore other entries like gt_clos and wrc_close */
			default:
				break;
			}
			/* point to next ptct entry */
			entry = (struct ptct_entry *)((uint64_t)entry + entry->size);
		}
		psram_area_top = round_page_up(psram_area_top);
	} else {
		pr_fatal("Cannot find PTCT pointer!!!!");
	}
}

/*
 * Function to initialize pSRAM. Both BSP and APs shall call this function to
 * make sure pSRAM is initialized, which is required by PTCM.
 * BSP:
 *	To parse PTCT and find the entry of PTCM command function
 * AP:
 *	Wait until BSP has done the parsing work, then call the PTCM ABI.
 *
 * Synchronization of AP and BSP is ensured, both inside and outside PTCM.
 * BSP shall be the last to finish the call.
 */
void init_psram(bool is_bsp)
{
	int32_t ptcm_ret_code;
	struct ptcm_header *header;
	ptcm_abi_func ptcm_command_func = NULL;
	static uint64_t init_psram_cpus_mask = (1UL << BSP_CPU_ID);

	/*
	 * When we shut down an RTVM, its pCPUs will be re-initialized
	 * we must ensure init_psram() will only be executed at the first time when a pcpu is booted
	 * That's why we add "!is_psram_initialized" as an condition.
	 */
	if (!is_psram_initialized && (acpi_ptct_tbl != NULL)) {
		/* TODO: We may use SMP call to flush TLB and do pSRAM initilization on APs */
		if (is_bsp) {
			parse_ptct();
			/* Clear the NX bit of PTCM area */
			ptcm_set_nx(false);
			bitmap_clear_lock(get_pcpu_id(), &init_psram_cpus_mask);
		}

		wait_sync_change(&init_psram_cpus_mask, 0UL);
		pr_info("PTCT is parsed by BSP");
		header = hpa2hva(ptcm_binary->address);
		pr_info("ptcm_bin_address:%llx, ptcm magic:%x, ptcm version:%x",
			ptcm_binary->address, header->magic, header->version);
		ASSERT(header->magic == PTCM_MAGIC, "Incorrect PTCM magic!");

		/* Flush the TLB, so that BSP/AP can execute the PTCM ABI */
		ptcm_flush_binary_tlb();
		ptcm_command_func = (ptcm_abi_func)(hpa2hva(ptcm_binary->address) + header->command_offset);
		pr_info("ptcm command function is found at %llx",ptcm_command_func);
		ptcm_ret_code = ptcm_command_func(PTCM_CMD_INIT_PSRAM, get_ptct_address());
		pr_info("ptcm initialization return %d", ptcm_ret_code);
		/* return 0 for success, -1 for failure */
		ASSERT(ptcm_ret_code == 0);

		if (is_bsp) {
			/* Restore the NX bit of PTCM area in page table */
			ptcm_set_nx(true);
		}

		bitmap_set_lock(get_pcpu_id(), &init_psram_cpus_mask);
		wait_sync_change(&init_psram_cpus_mask, ALL_CPUS_MASK);
		/* Flush the TLB on BSP and all APs to restore the NX for pSRAM area */
		ptcm_flush_binary_tlb();

		if (is_bsp) {
			is_psram_initialized = true;
			pr_info("BSP pSRAM has been initialized\n");
		}
	}
}
#else
void set_ptct_tbl(__unused void *ptct_tbl_addr)
{
}

void init_psram(__unused bool is_bsp)
{
}
#endif
