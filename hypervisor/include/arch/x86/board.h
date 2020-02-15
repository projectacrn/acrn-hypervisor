/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

#include <types.h>
#include <misc_cfg.h>
#include <host_pm.h>
#include <pci.h>

/* forward declarations */
struct acrn_vm;

struct platform_clos_info {
	uint32_t clos_mask;
	uint32_t msr_index;
};

extern struct dmar_info plat_dmar_info;
extern struct platform_clos_info platform_l2_clos_array[MAX_PLATFORM_CLOS_NUM];
extern struct platform_clos_info platform_l3_clos_array[MAX_PLATFORM_CLOS_NUM];
extern const struct cpu_state_table board_cpu_state_tbl;
extern const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];

/* board specific functions */
void create_prelaunched_vm_e820(struct acrn_vm *vm);

#endif /* BOARD_H */
