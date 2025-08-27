/*
 * Copyright (C) 2023-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Authors:
 *   Haicheng Li <haicheng.li@intel.com>
 */

#ifndef VM_CONFIG_H_
#define VM_CONFIG_H_
#include <board_info.h>

#define MAX_VCPUS_PER_VM  MAX_PCPU_NUM
#define CONFIG_MAX_VM_NUM 16U

/* TODO: To be moved in later patches */
enum os_kernel_type {
	DUMMY,
};

/* TODO: Dummy, to be removed */
#include <schedule.h>
struct acrn_vm_config {
	struct sched_params sched_params;		/* Scheduler params for vCPUs of this VM */
};
static inline struct acrn_vm_config *get_vm_config(__unused uint16_t vm_id) {
	return NULL;
}

#endif /* VM_CONFIG_H_ */
