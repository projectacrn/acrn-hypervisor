/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <hypervisor.h>

#ifdef CONFIG_VM0_DESC

/* Number of CPUs in VM0 */
#define VM0_NUM_CPUS    1

/* Logical CPU IDs assigned to VM0 */
uint16_t VM0_CPUS[VM0_NUM_CPUS] = {0U};

struct vm_description vm0_desc = {
	.vm_hw_num_cores = VM0_NUM_CPUS,
	.vm_pcpu_ids = &VM0_CPUS[0],
};

#else

struct vm_description vm0_desc;

#endif // CONFIG_VM0_DESC
