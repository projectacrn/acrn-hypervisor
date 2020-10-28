/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <types.h>
#include <bits.h>
#include <logmsg.h>
#include <misc_cfg.h>
#include <mmu.h>
#include <ptcm.h>


/* is_psram_initialized is used to tell whether psram is successfully initialized for all cores */
volatile bool is_psram_initialized = false;

#ifdef CONFIG_PSRAM_ENABLED

static struct ptct_entry_data_ptcm_binary *ptcm_binary = NULL;

static struct acpi_table_header *acpi_ptct_tbl = NULL;

static inline void *get_ptct_address()
{
	return (void *)acpi_ptct_tbl + sizeof(*acpi_ptct_tbl);
}

static void parse_ptct(void)
{
	/* TODO: Will add in the next patch */
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
		if (is_bsp) {
			parse_ptct();
			bitmap_clear_lock(get_pcpu_id(), &init_psram_cpus_mask);
		}

		wait_sync_change(&init_psram_cpus_mask, 0UL);
		pr_info("PTCT is parsed by BSP");
		header = hpa2hva(ptcm_binary->address);
		pr_info("ptcm_bin_address:%llx, ptcm magic:%x, ptcm version:%x",
			ptcm_binary->address, header->magic, header->version);
		ASSERT(header->magic == PTCM_MAGIC, "Incorrect PTCM magic!");

		ptcm_command_func = (ptcm_abi_func)(hpa2hva(ptcm_binary->address) + header->command_offset);
		pr_info("ptcm command function is found at %llx",ptcm_command_func);
		ptcm_ret_code = ptcm_command_func(PTCM_CMD_INIT_PSRAM, get_ptct_address());
		pr_info("ptcm initialization return %d", ptcm_ret_code);
		/* return 0 for success, -1 for failure */
		ASSERT(ptcm_ret_code == 0);

		bitmap_set_lock(get_pcpu_id(), &init_psram_cpus_mask);
		wait_sync_change(&init_psram_cpus_mask, ALL_CPUS_MASK);

		if (is_bsp) {
			is_psram_initialized = true;
			pr_info("BSP pSRAM has been initialized\n");
		}
	}
}
#else
void init_psram(__unused bool is_bsp)
{
}
#endif
