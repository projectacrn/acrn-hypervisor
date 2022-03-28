/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

#include <types.h>
#include <board_info.h>
#include <asm/host_pm.h>
#include <pci.h>
#include <misc_cfg.h>

/* forward declarations */
struct acrn_vm;

/* user configured mask and MSR info for each CLOS*/
union clos_config {
	uint16_t mba_delay;
	uint32_t clos_mask;
};

struct vmsix_on_msi_info {
	union pci_bdf bdf;
	uint64_t mmio_base;
};

extern struct dmar_info plat_dmar_info;

#ifdef CONFIG_RDT_ENABLED
extern union clos_config platform_l2_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];
extern union clos_config platform_l3_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];
extern union clos_config platform_mba_clos_array[MAX_MBA_CLOS_NUM_ENTRIES];
#endif

extern const struct cpu_state_table board_cpu_state_tbl;
extern const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];
extern const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];

#endif /* BOARD_H */
