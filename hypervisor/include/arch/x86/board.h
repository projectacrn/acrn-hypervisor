/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef BOARD_H
#define BOARD_H

#include <types.h>
#include <host_pm.h>

/* forward declarations */
struct acrn_vm;

struct platform_clos_info {
	uint32_t clos_mask;
	uint32_t msr_index;
};

extern struct platform_clos_info platform_clos_array[];
extern uint16_t platform_clos_num;
extern const struct cpu_state_table board_cpu_state_tbl;

/* board specific functions */
void create_prelaunched_vm_e820(struct acrn_vm *vm);

#endif /* BOARD_H */
