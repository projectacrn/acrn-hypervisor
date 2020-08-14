/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

#include <types.h>
#include <board_info.h>
#include <host_pm.h>
#include <pci.h>

/* forward declarations */
struct acrn_vm;

struct platform_clos_info {
	uint16_t mba_delay;
	uint32_t clos_mask;
	uint32_t msr_index;
};

struct vmsix_on_msi_info {
	union pci_bdf bdf;
	uint64_t mmio_base;
};

extern struct dmar_info plat_dmar_info;

#ifdef CONFIG_RDT_ENABLED
extern struct platform_clos_info platform_l2_clos_array[MAX_PLATFORM_CLOS_NUM];
extern struct platform_clos_info platform_l3_clos_array[MAX_PLATFORM_CLOS_NUM];
extern struct platform_clos_info platform_mba_clos_array[MAX_PLATFORM_CLOS_NUM];
#endif

extern const struct cpu_state_table board_cpu_state_tbl;
extern const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];
extern const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];

#endif /* BOARD_H */
